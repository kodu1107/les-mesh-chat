"use strict";

const elements = {
    serviceStatus:
        document.querySelector("#service-status"),
    callsign:
        document.querySelector("#node-callsign"),
    nodeId:
        document.querySelector("#node-id"),
    address:
        document.querySelector("#node-address"),
    version:
        document.querySelector("#service-version")
};

function setConnectionState(state, text) {
    elements.serviceStatus.className =
        `status status-${state}`;

    elements.serviceStatus.textContent = text;
}

async function refreshStatus() {
    try {
        const response = await fetch(
            "/api/v1/status",
            {
                cache: "no-store",
                headers: {
                    "Accept": "application/json"
                }
            }
        );

        if (!response.ok) {
            throw new Error(
                `HTTP ${response.status}`
            );
        }

        const status = await response.json();

        elements.callsign.textContent =
            status.callsign;

        elements.nodeId.textContent =
            status.node_id;

        elements.address.textContent =
            `${status.bind_address}:${status.port}`;

        elements.version.textContent =
            status.version;

        setConnectionState("online", "ONLINE");
    } catch (error) {
        console.error(
            "Unable to refresh node status:",
            error
        );

        setConnectionState(
            "offline",
            "CONNECTION LOST"
        );
    }
}

refreshStatus();

window.setInterval(
    refreshStatus,
    5000
);