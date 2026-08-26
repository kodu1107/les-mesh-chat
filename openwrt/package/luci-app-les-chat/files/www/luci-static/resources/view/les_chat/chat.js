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
			'data-message-id': message.id || ''
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
		const items = Array.isArray(messages) ? messages : [];
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
		list.appendChild(this.messageNode(message));
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
				jump.style.display = 'none';
				list.scrollTop = list.scrollHeight;
			}, this)
		}, [ _('New messages') ]);
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
			if (this.pinnedToBottom)
				jump.style.display = 'none';
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
				input.disabled = true;
				send.disabled = true;
				hint.textContent = _('Sending…');
				hint.className = 'les-chat-hint';

				return callSend(body).then(L.bind(function(result) {
					if (result && result.error)
						throw new Error(result.error);
					if (result && result.message)
						this.appendMessage(list, result.message);
					input.value = '';
					hint.textContent = _('Sent');
					this.pinnedToBottom = true;
					list.scrollTop = list.scrollHeight;
				}, this)).catch(function(error) {
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
					_('Internal scroll — page length stays fixed')
				])
			]),
			list,
			jump,
			E('div', { 'class': 'les-chat-composer' }, [ form ]),
			hint
		]);

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
					jump.style.display = '';

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
