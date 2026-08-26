'use strict';
'require view';
'require rpc';
'require poll';
'require view.les_chat.common as common';

const callStatus = rpc.declare({
	object: 'luci.leschat',
	method: 'status'
});

const callPeers = rpc.declare({
	object: 'luci.leschat',
	method: 'peers'
});

const callMessages = rpc.declare({
	object: 'luci.leschat',
	method: 'messages'
});

const callSend = rpc.declare({
	object: 'luci.leschat',
	method: 'send',
	params: [ 'body' ]
});

function nearBottom(node) {
	if (!node)
		return true;
	return (node.scrollHeight - node.scrollTop - node.clientHeight) < 48;
}

function messageTime(message) {
	const value = Number(message && message.created_at_ms);
	return Number.isFinite(value) ? value : 0;
}

function messageSequence(message) {
	const value = Number(message && message.sequence);
	return Number.isFinite(value) ? value : 0;
}

function compareMessages(left, right) {
	const leftTime = messageTime(left);
	const rightTime = messageTime(right);
	if (leftTime !== rightTime)
		return leftTime < rightTime ? -1 : 1;

	const leftOrigin = String((left && (left.origin || left.callsign)) || '');
	const rightOrigin = String((right && (right.origin || right.callsign)) || '');
	if (leftOrigin !== rightOrigin)
		return leftOrigin < rightOrigin ? -1 : 1;

	const leftSequence = messageSequence(left);
	const rightSequence = messageSequence(right);
	if (leftSequence !== rightSequence)
		return leftSequence < rightSequence ? -1 : 1;

	const leftId = String((left && (left.id || left.message_id)) || '');
	const rightId = String((right && (right.id || right.message_id)) || '');
	if (leftId === rightId)
		return 0;
	return leftId < rightId ? -1 : 1;
}

/*
 * rpcd normally returns the method object directly, but older LuCI/rpcd
 * combinations can wrap a single result in an array (or leave it as a JSON
 * string).  Keep the send path tolerant of both forms so a successful daemon
 * write is not shown as a client-side failure.
 */
function normalizeRpcResult(result) {
	let value = result;
	while (Array.isArray(value) && value.length === 1)
		value = value[0];

	if (typeof value === 'string') {
		try {
			value = JSON.parse(value);
		}
		catch (error) {
			return {};
		}
	}

	return value && typeof value === 'object' ? value : {};
}

return view.extend({
	composing: false,
	sending: false,
	pinnedToBottom: true,
	liveRunning: false,
	localNodeId: '',
	knownIds: {},

	load: function() {
		common.loadCss();
		return Promise.all([
			callStatus().catch(function(error) { return { rpcError: error }; }),
			callPeers().catch(function() { return { peers: [] }; }),
			callMessages().catch(function() { return { messages: [] }; })
		]);
	},

	messageNode: function(message) {
		const local = message.origin === this.localNodeId;
		const article = E('article', {
			'class': 'les-chat-msg ' + (local ? 'les-chat-msg-local' : 'les-chat-msg-remote'),
			'data-message-id': message.id || '',
			'data-message-origin': message.origin || message.callsign || '',
			'data-message-sequence': message.sequence != null ? String(message.sequence) : '0',
			'data-message-time': message.created_at_ms != null ? String(message.created_at_ms) : '0'
		}, [
			E('header', { 'class': 'les-chat-msg-header' }, [
				E('strong', { 'class': 'les-chat-msg-callsign' }, [
					message.callsign || '—'
				]),
				E('time', { 'class': 'les-chat-msg-time' }, [
					common.formatTime(message.created_at_ms)
				])
			]),
			E('p', { 'class': 'les-chat-msg-body' }, [
				message.body || ''
			])
		]);
		return article;
	},

	fillMessages: function(list, messages, emptyTitle, emptyDetail) {
		const items = Array.isArray(messages) ? messages.slice().sort(compareMessages) : [];
		this.knownIds = {};
		if (items.length === 0) {
			L.dom.content(list, common.empty(emptyTitle, emptyDetail));
			return;
		}
		const fragment = document.createDocumentFragment();
		items.forEach(L.bind(function(message) {
			if (message && message.id)
				this.knownIds[message.id] = true;
			fragment.appendChild(this.messageNode(message));
		}, this));
		L.dom.content(list, fragment);
		if (this.pinnedToBottom)
			list.scrollTop = list.scrollHeight;
	},

	appendMessage: function(list, message) {
		if (!message || !message.id || this.knownIds[message.id])
			return false;
		this.knownIds[message.id] = true;
		const empty = list.querySelector('.les-chat-empty');
		if (empty)
			empty.remove();

		const node = this.messageNode(message);
		const existing = list.querySelectorAll('.les-chat-msg');
		for (let i = 0; i < existing.length; i++) {
			const existingMessage = {
				id: existing[i].getAttribute('data-message-id') || '',
				origin: existing[i].getAttribute('data-message-origin') || '',
				sequence: existing[i].getAttribute('data-message-sequence') || '0',
				created_at_ms: existing[i].getAttribute('data-message-time') || '0'
			};
			if (compareMessages(message, existingMessage) < 0) {
				list.insertBefore(node, existing[i]);
				return true;
			}
		}
		list.appendChild(node);
		return true;
	},

	render: function(data) {
		const status = data[0] || {};
		const peers = data[1] || {};
		const history = data[2] || {};
		const rpcError = status.rpcError;
		this.liveRunning = common.isTrue(status.running) &&
			common.isTrue(status.healthz_ok) && !rpcError;
		this.localNodeId = status.node_id || '';
		let hasNewMessages = false;

		const list = E('div', {
			'id': 'les-chat-messages',
			'class': 'les-chat-messages',
			'aria-live': 'polite'
		});
		const jump = E('button', {
			'class': 'cbi-button les-chat-jump',
			'style': 'display:none',
			'click': L.bind(function() {
				this.pinnedToBottom = true;
				hasNewMessages = false;
				jump.style.display = 'none';
				list.scrollTop = list.scrollHeight;
				jump.textContent = _('Jump to latest');
			}, this)
		}, [ _('Jump to latest') ]);
		const input = E('input', {
			'id': 'les-chat-input',
			'type': 'text',
			'maxlength': '2048',
			'autocomplete': 'off',
			'placeholder': _('Type a message')
		});
		const send = E('button', { 'type': 'submit' }, [ _('Send') ]);
		const hint = E('p', {
			'id': 'les-chat-hint',
			'class': 'les-chat-hint',
			'role': 'status'
		});

		if (!this.liveRunning) {
			input.disabled = true;
			send.disabled = true;
		}

		list.addEventListener('scroll', L.bind(function() {
			this.pinnedToBottom = nearBottom(list);
			if (this.pinnedToBottom) {
				hasNewMessages = false;
				jump.style.display = 'none';
			}
			else {
				jump.textContent = hasNewMessages ? _('New messages') : _('Jump to latest');
				jump.style.display = '';
			}
		}, this));

		input.addEventListener('compositionstart', L.bind(function() {
			this.composing = true;
		}, this));
		input.addEventListener('compositionend', L.bind(function() {
			this.composing = false;
		}, this));
		input.addEventListener('keydown', L.bind(function(event) {
			if (event.key === 'Enter' &&
			    (event.isComposing || this.composing || event.keyCode === 229))
				event.preventDefault();
		}, this));

		const form = E('form', {
			'class': 'les-chat-form',
			'submit': L.bind(function(event) {
				event.preventDefault();
				if (this.composing || this.sending)
					return;

				const body = input.value.replace(/^\s+|\s+$/g, '');
				if (!body) {
					hint.textContent = _('Enter a message.');
					hint.className = 'les-chat-hint les-chat-hint-error';
					return;
				}

				this.sending = true;
				const knownBeforeSend = Object.assign({}, this.knownIds);
				input.disabled = true;
				send.disabled = true;
				hint.textContent = _('Sending…');
				hint.className = 'les-chat-hint';

				const markSent = L.bind(function(result) {
					const response = normalizeRpcResult(result);
					if (response.error)
						throw new Error(response.error);
					if (response.message)
						this.appendMessage(list, response.message);
					input.value = '';
					hint.textContent = _('Sent');
					this.pinnedToBottom = true;
					hasNewMessages = false;
					list.scrollTop = list.scrollHeight;
					jump.style.display = 'none';
				}, this);

				/*
				 * The daemon stores before replying.  If rpcd/wget drops the reply,
				 * confirm a newly assigned message ID from local history before
				 * reporting an error.  Do not compare browser and node timestamps:
				 * an offline mesh can legitimately have different clocks.
				 * This avoids the misleading "Could not send" state seen when the
				 * message is already visible after the next poll.
				 */
				const confirmStored = L.bind(function(error) {
					return callMessages().then(L.bind(function(history) {
						const response = normalizeRpcResult(history);
						const messages = Array.isArray(response.messages)
							? response.messages : [];
						let stored = null;
						for (let i = messages.length - 1; i >= 0; i--) {
							const message = messages[i];
							if (!message || message.body !== body)
								continue;
							if (this.localNodeId && message.origin !== this.localNodeId)
								continue;
							const messageId = message.id || message.message_id || '';
							if (!messageId || knownBeforeSend[messageId])
								continue;
							stored = message;
							break;
						}

						if (stored) {
							markSent({ message: stored });
							return;
						}

						throw error;
					}, this));
				}, this);

				return callSend(body).then(markSent).catch(confirmStored).catch(function(error) {
					hint.textContent = _('Could not send the message.');
					hint.className = 'les-chat-hint les-chat-hint-error';
					console.error(error);
				}).finally(L.bind(function() {
					this.sending = false;
					if (this.liveRunning) {
						input.disabled = false;
						send.disabled = false;
						input.focus();
					}
				}, this));
			}, this)
		}, [ input, send ]);

		let emptyTitle = _('No messages yet');
		let emptyDetail = _('Send the first message on this node.');
		if (rpcError) {
			emptyTitle = _('Cannot reach the chat service');
			emptyDetail = _('LuCI could not read the local daemon. The standalone page on port 7777 still works if the process is up.');
		} else if (!this.liveRunning) {
			emptyTitle = _('Chat service is stopped');
			emptyDetail = _('Enable les-chatd in Settings, or start /etc/init.d/les-chatd.');
		}

		this.fillMessages(list, history.messages, emptyTitle, emptyDetail);

		const body = E('div', { 'class': 'les-chat-stage' }, [
			E('div', { 'class': 'les-chat-channel' }, [
				E('div', {}, [
					E('p', { 'class': 'les-chat-eyebrow' }, [ _('Channel') ]),
					E('strong', {}, [ 'les-manet' ])
				]),
				E('span', { 'class': 'les-chat-channel-name' }, [
					_('Live updates')
				])
			]),
			list,
			jump,
			E('div', { 'class': 'les-chat-composer' }, [ form ]),
			hint
		]);
		if (this.pinnedToBottom) {
			window.setTimeout(function() {
				list.scrollTop = list.scrollHeight;
			}, 0);
		}

		poll.add(L.bind(function() {
			if (!document.getElementById('les-chat-messages'))
				return Promise.resolve();

			return Promise.all([
				callStatus().catch(function(error) { return { rpcError: error }; }),
				callPeers().catch(function() { return { peers: [] }; }),
				callMessages().catch(function(error) { return { rpcError: error, messages: [] }; })
			]).then(L.bind(function(next) {
				const nextStatus = next[0] || {};
				const nextPeers = next[1] || {};
				const nextHistory = next[2] || {};
				this.localNodeId = nextStatus.node_id || this.localNodeId;
				const wasPinned = this.pinnedToBottom;
				const incoming = Array.isArray(nextHistory.messages)
					? nextHistory.messages : [];
				let added = false;
				incoming.forEach(L.bind(function(message) {
					if (this.appendMessage(list, message))
						added = true;
				}, this));
				if (added && wasPinned)
					list.scrollTop = list.scrollHeight;
				else if (added && !wasPinned)
					hasNewMessages = true;
				if (!nearBottom(list)) {
					jump.textContent = hasNewMessages ? _('New messages') : _('Jump to latest');
					jump.style.display = '';
				}

				const root = document.querySelector('.les-chat-app');
				if (!root)
					return;
				const state = common.connectionState(nextStatus, nextStatus.rpcError);
				const callsign = root.querySelector('.les-chat-callsign');
				const nodeId = root.querySelector('.les-chat-node-id');
				const badge = root.querySelector('.les-chat-badge');
				const peerMetric = root.querySelector('.les-chat-metrics dd');
				const daemonMetric = root.querySelectorAll('.les-chat-metrics dd')[1];
				if (callsign)
					callsign.textContent = nextStatus.callsign || '—';
				if (nodeId)
					nodeId.textContent = _('Node ID') + ': ' + (nextStatus.node_id || '—');
				if (badge) {
					badge.className = 'les-chat-badge les-chat-badge-' + state.id;
					badge.textContent = state.label;
				}
				if (peerMetric)
					peerMetric.textContent = Array.isArray(nextPeers.peers)
						? String(nextPeers.peers.length) : '—';
				if (daemonMetric)
					daemonMetric.textContent = state.label;

				this.liveRunning = common.isTrue(nextStatus.running) &&
					common.isTrue(nextStatus.healthz_ok) && !nextStatus.rpcError;
				if (!this.sending) {
					input.disabled = !this.liveRunning;
					send.disabled = !this.liveRunning;
				}
			}, this));
		}, this), 2);

		return common.shell({
			title: _('LES Mesh Chat'),
			status: status,
			state: common.connectionState(status, rpcError),
			peerCount: Array.isArray(peers.peers) ? peers.peers.length : 0,
			rpcError: rpcError,
			body: body
		});
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
