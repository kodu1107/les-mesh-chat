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
        document.querySelector("#service-version"),
    messageList:
        document.querySelector("#message-list"),
    messageForm:
        document.querySelector("#message-form"),
    messageInput:
        document.querySelector("#message-input"),
    sendButton:
        document.querySelector("#send-button"),
    composerStatus:
        document.querySelector("#composer-status")
};

let composing = false;
let sending = false;

function setConnectionState(state, text) {
    elements.serviceStatus.className =
        `status status-${state}`;

    elements.serviceStatus.textContent = text;
}

function setComposerStatus(text, isError = false) {
    elements.composerStatus.textContent = text;

    elements.composerStatus.className =
        isError
            ? "composer-status composer-status-error"
            : "composer-status";
}

function formatMessageTime(createdAtMs) {
    const date = new Date(createdAtMs);

    if (Number.isNaN(date.getTime())) {
        return "";
    }

    return new Intl.DateTimeFormat(
        "ko-KR",
        {
            hour: "2-digit",
            minute: "2-digit",
            second: "2-digit"
        }
    ).format(date);
}

function createMessageElement(message) {
    const article = document.createElement("article");
    article.className = "message";
    article.dataset.messageId = message.id;

    const header = document.createElement("header");
    header.className = "message-header";

    const callsign = document.createElement("strong");
    callsign.className = "message-callsign";
    callsign.textContent = message.callsign;

    const time = document.createElement("time");
    time.className = "message-time";
    time.dateTime = new Date(
        message.created_at_ms
    ).toISOString();
    time.textContent = formatMessageTime(
        message.created_at_ms
    );

    const body = document.createElement("p");
    body.className = "message-body";

    // textContent를 사용해 메시지 안의 HTML이
    // 브라우저에서 실행되지 않도록 합니다.
    body.textContent = message.body;

    header.append(callsign, time);
    article.append(header, body);

    return article;
}

function renderMessages(messages) {
    const fragment = document.createDocumentFragment();

    if (messages.length === 0) {
        const empty = document.createElement("div");
        empty.className = "empty-message";

        const title = document.createElement("strong");
        title.textContent = "아직 메시지가 없습니다";

        const description = document.createElement("p");
        description.textContent =
            "첫 번째 메시지를 전송해 보세요.";

        empty.append(title, description);
        fragment.append(empty);
    } else {
        for (const message of messages) {
            fragment.append(
                createMessageElement(message)
            );
        }
    }

    elements.messageList.replaceChildren(fragment);
    elements.messageList.scrollTop =
        elements.messageList.scrollHeight;
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

async function refreshMessages() {
    try {
        const response = await fetch(
            "/api/v1/messages",
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

        const result = await response.json();

        if (!Array.isArray(result.messages)) {
            throw new Error(
                "Invalid messages response"
            );
        }

        renderMessages(result.messages);
    } catch (error) {
        console.error(
            "Unable to refresh messages:",
            error
        );

        setComposerStatus(
            "메시지 목록을 불러오지 못했습니다.",
            true
        );
    }
}

async function sendMessage(body) {
    const response = await fetch(
        "/api/v1/messages",
        {
            method: "POST",
            headers: {
                "Accept": "application/json",
                "Content-Type": "application/json"
            },
            body: JSON.stringify({body})
        }
    );

    const result = await response.json();

    if (!response.ok) {
        throw new Error(
            result.error ?? `HTTP ${response.status}`
        );
    }

    return result.message;
}

elements.messageForm.addEventListener(
    "submit",
    async (event) => {
        event.preventDefault();

        if (composing || sending) {
            return;
        }

        const body = elements.messageInput.value.trim();

        if (body.length === 0) {
            setComposerStatus(
                "메시지를 입력하세요.",
                true
            );
            return;
        }

        sending = true;
        elements.sendButton.disabled = true;
        elements.messageInput.disabled = true;
        setComposerStatus("전송 중...");

        try {
            await sendMessage(body);

            elements.messageInput.value = "";
            setComposerStatus("전송 완료");

            await refreshMessages();
        } catch (error) {
            console.error(
                "Unable to send message:",
                error
            );

            setComposerStatus(
                "메시지를 전송하지 못했습니다.",
                true
            );
        } finally {
            sending = false;
            elements.sendButton.disabled = false;
            elements.messageInput.disabled = false;
            elements.messageInput.focus();
        }
    }
);

elements.messageInput.addEventListener(
    "compositionstart",
    () => {
        composing = true;
    }
);

elements.messageInput.addEventListener(
    "compositionend",
    () => {
        composing = false;
    }
);

elements.messageInput.addEventListener(
    "keydown",
    (event) => {
        if (
            event.key === "Enter" &&
            (event.isComposing ||
             composing ||
             event.keyCode === 229)
        ) {
            event.preventDefault();
        }
    }
);

refreshStatus();
refreshMessages();

window.setInterval(refreshStatus, 5000);
window.setInterval(refreshMessages, 2000);