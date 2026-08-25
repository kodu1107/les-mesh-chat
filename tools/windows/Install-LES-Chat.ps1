[CmdletBinding()]
param(
    [string]$NodeAddress,
    [string]$Callsign,
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

if ([string]::IsNullOrWhiteSpace($NodeAddress)) {
    $NodeAddress = Read-Host "OpenMANET node IP address"
}
if ($NodeAddress -notmatch '^[A-Za-z0-9][A-Za-z0-9.-]*$') {
    throw "NodeAddress must be an IPv4 address or DNS hostname."
}
if ([string]::IsNullOrWhiteSpace($Callsign)) {
    $Callsign = Read-Host "Callsign (leave empty to keep auto)"
}
if ($Callsign -and $Callsign -notmatch '^[A-Za-z0-9._-]{1,32}$') {
    throw "Callsign may contain only letters, numbers, dot, underscore, and dash."
}
if ($IdentityFile -and -not (Test-Path -LiteralPath $IdentityFile -PathType Leaf)) {
    throw "SSH identity file does not exist: $IdentityFile"
}

$sshOptions = @()
if ($IdentityFile) {
    $sshOptions += @("-i", $IdentityFile)
}
$target = "root@$NodeAddress"

Write-Host "Checking $target ..."
$probe = & ssh.exe @sshOptions $target '. /etc/openwrt_release; printf "RELEASE=%s\n" "$DISTRIB_RELEASE"; opkg print-architecture'
if ($LASTEXITCODE -ne 0) {
    throw "Unable to inspect the OpenMANET node over SSH."
}
$releaseLine = $probe | Where-Object { $_ -match '^RELEASE=' } | Select-Object -First 1
$openWrtRelease = $releaseLine -replace '^RELEASE=', ''
if ($openWrtRelease -ne "24.10.2") {
    throw "Unsupported OpenWrt release: $openWrtRelease (expected 24.10.2)"
}
$architecture = if ($probe -match 'aarch64_cortex-a76') {
    "aarch64_cortex-a76"
} elseif ($probe -match 'aarch64_cortex-a72') {
    "aarch64_cortex-a72"
} else {
    throw "The node is not a supported Raspberry Pi 4 or Pi 5 target."
}

$headers = @{ "User-Agent" = "LES-Mesh-Chat-Windows-Installer" }
$releaseUri = if ($ReleaseTag -eq "latest") {
    "https://api.github.com/repos/$Repository/releases/latest"
} else {
    $encodedTag = [Uri]::EscapeDataString($ReleaseTag)
    "https://api.github.com/repos/$Repository/releases/tags/$encodedTag"
}
Write-Host "Finding the $ReleaseTag release for $architecture ..."
$releaseInfo = Invoke-RestMethod -Uri $releaseUri -Headers $headers
$assetPattern = '^les-chat-offline-.*-' + [Regex]::Escape($architecture) + '\.tar\.gz$'
$assets = @($releaseInfo.assets | Where-Object { $_.name -match $assetPattern })
if ($assets.Count -ne 1) {
    throw "Expected one offline bundle for $architecture, found $($assets.Count)."
}

$temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) ("les-chat-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null
try {
    $localArchive = Join-Path $temporaryDirectory $assets[0].name
    Write-Host "Downloading $($assets[0].name) ..."
    Invoke-WebRequest -Uri $assets[0].browser_download_url -Headers $headers -OutFile $localArchive

    Write-Host "Copying the signed bundle to $target ..."
    Invoke-Native "scp.exe" ($sshOptions + @($localArchive, "${target}:/tmp/les-chat-offline.tar.gz"))

    $remoteInstall = 'set -eu; rm -rf /tmp/les-chat-offline-install; mkdir -p /tmp/les-chat-offline-install; tar -xzf /tmp/les-chat-offline.tar.gz -C /tmp/les-chat-offline-install; bundle=$(find /tmp/les-chat-offline-install -mindepth 1 -maxdepth 1 -type d | head -n 1); test "$(usign -F -p "$bundle/opkg.pub")" = 9db1776b78018b98; usign -V -m "$bundle/SHA256SUMS" -p "$bundle/opkg.pub" -x "$bundle/SHA256SUMS.sig"; (cd "$bundle" && sha256sum -c SHA256SUMS); "$bundle/install.sh"'
    if ($Callsign) {
        $remoteInstall += "; uci set les-chat.main.callsign='$Callsign'; uci commit les-chat; /etc/init.d/les-chatd restart"
    }
    $remoteInstall += '; rm -rf /tmp/les-chat-offline-install /tmp/les-chat-offline.tar.gz'

    Write-Host "Installing and verifying on the node ..."
    Invoke-Native "ssh.exe" ($sshOptions + @($target, $remoteInstall))
    Write-Host "LES Mesh Chat installation completed: http://${NodeAddress}:7777/"
} finally {
    Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
