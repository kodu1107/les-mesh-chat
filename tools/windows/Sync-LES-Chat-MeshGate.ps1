[CmdletBinding()]
param(
    [string]$MeshGateAddress,
    [ValidateRange(1, 65535)][int]$Port = 8088,
    [string]$FirewallZone = "ahwlan",
    [string]$ReleaseTag = "latest",
    [string]$Repository = "kodu1107/les-mesh-chat",
    [string]$IdentityFile
)

$ErrorActionPreference = "Stop"

function Require-Command([string]$Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required Windows OpenSSH command is missing: $Name"
    }
}

function Invoke-Native([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE"
    }
}

Require-Command "ssh.exe"
Require-Command "scp.exe"

if ([string]::IsNullOrWhiteSpace($MeshGateAddress)) {
    $MeshGateAddress = Read-Host "MeshGate IP address"
}
if ($MeshGateAddress -notmatch '^[A-Za-z0-9][A-Za-z0-9.-]*$') {
    throw "MeshGateAddress must be an IPv4 address or DNS hostname."
}
if ($FirewallZone -notmatch '^[A-Za-z0-9_-]+$') {
    throw "FirewallZone contains unsupported characters."
}
if ($IdentityFile -and -not (Test-Path -LiteralPath $IdentityFile -PathType Leaf)) {
    throw "SSH identity file does not exist: $IdentityFile"
}

$sshOptions = @()
if ($IdentityFile) {
    $sshOptions += @("-i", $IdentityFile)
}
$target = "root@$MeshGateAddress"

Write-Host "Checking $target ..."
Invoke-Native "ssh.exe" ($sshOptions + @($target, 'test -r /etc/openwrt_release && command -v usign >/dev/null'))

$headers = @{ "User-Agent" = "LES-Mesh-Chat-MeshGate-Sync" }
$releaseUri = if ($ReleaseTag -eq "latest") {
    "https://api.github.com/repos/$Repository/releases/latest"
} else {
    $encodedTag = [Uri]::EscapeDataString($ReleaseTag)
    "https://api.github.com/repos/$Repository/releases/tags/$encodedTag"
}
$releaseInfo = Invoke-RestMethod -Uri $releaseUri -Headers $headers
$assets = @($releaseInfo.assets | Where-Object { $_.name -match '^les-chat-meshgate-feed-.*\.tar\.gz$' })
if ($assets.Count -ne 1) {
    throw "Expected one MeshGate feed bundle, found $($assets.Count)."
}

$temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) ("les-chat-feed-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null
try {
    $localArchive = Join-Path $temporaryDirectory $assets[0].name
    Write-Host "Downloading $($assets[0].name) ..."
    Invoke-WebRequest -Uri $assets[0].browser_download_url -Headers $headers -OutFile $localArchive

    Write-Host "Copying the signed feed to $target ..."
    Invoke-Native "scp.exe" ($sshOptions + @($localArchive, "${target}:/tmp/les-chat-meshgate-feed.tar.gz"))

    $remoteSetup = 'set -eu; rm -rf /tmp/les-chat-meshgate-setup; mkdir -p /tmp/les-chat-meshgate-setup; tar -xzf /tmp/les-chat-meshgate-feed.tar.gz -C /tmp/les-chat-meshgate-setup; bundle=$(find /tmp/les-chat-meshgate-setup -mindepth 1 -maxdepth 1 -type d | head -n 1); test "$(usign -F -p "$bundle/opkg.pub")" = 9db1776b78018b98; usign -V -m "$bundle/SHA256SUMS" -p "$bundle/opkg.pub" -x "$bundle/SHA256SUMS.sig"; (cd "$bundle" && sha256sum -c SHA256SUMS); "$bundle/setup.sh"' + " $Port $FirewallZone" + '; rm -rf /tmp/les-chat-meshgate-setup /tmp/les-chat-meshgate-feed.tar.gz'
    Write-Host "Installing the local feed service ..."
    Invoke-Native "ssh.exe" ($sshOptions + @($target, $remoteSetup))

    $feedUrl = "http://${MeshGateAddress}:$Port"
    Write-Host "MeshGate feed sync completed: $feedUrl"
    Write-Host "Run this on each attached OpenMANET node:"
    Write-Host "wget -qO- $feedUrl/install.sh | sh -s -- $feedUrl"
} finally {
    Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
