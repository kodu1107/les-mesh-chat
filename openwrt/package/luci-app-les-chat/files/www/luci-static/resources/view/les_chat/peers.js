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

function renderList(status, peers, rpcError) {
	const running = common.isTrue(status.running);
	const items = Array.isArray(peers.peers) ? peers.peers : [];

	if (rpcError)
		return common.empty(
			_('Cannot read peers'),
			_('LuCI lost contact with the local daemon. Wait a few seconds or check /etc/init.d/les-chatd status.')
		);

	if (!running)
		return common.empty(
			_('Chat service is stopped'),
			_('Peers cannot announce while les-chatd is down. Start the service in Settings.')
		);

	if (items.length === 0)
		return common.empty(
			_('No peers online'),
			_('No announce seen in the last 20 seconds. Wait up to 10 seconds, then check that discovery_interface is br-ahwlan on MeshGate and that UDP 7777 is allowed on ahwlan.')
		);

	const cards = E('div', { 'class': 'les-chat-cards les-chat-scroll' });
	items.forEach(function(peer) {
		cards.appendChild(E('div', { 'class': 'les-chat-card' }, [
			E('strong', {}, [ peer.callsign || '—' ]),
			E('span', {}, [ _('Node ID') + ': ' + (peer.node_id || '—') ]),
			E('span', {}, [
				(peer.address || '—') + ':' +
				(peer.http_port != null ? String(peer.http_port) : '—')
			]),
			E('span', {}, [ _('Version') + ': ' + (peer.app_version || '—') ]),
			E('span', {}, [ _('Last seen') + ': ' + common.formatLastSeen(peer.last_seen_ms) ])
		]));
	});
	return cards;
}

return view.extend({
	load: function() {
		common.loadCss();
		return Promise.all([
			callStatus().catch(function(error) { return { rpcError: error }; }),
			callPeers().catch(function(error) { return { rpcError: error, peers: [] }; })
		]);
	},

	render: function(data) {
		const container = E('div', { 'id': 'les-chat-peers-body' });
		const initialStatus = data[0] || {};
		const initialPeers = data[1] || {};

		const paint = function(status, peers) {
			const rpcError = status.rpcError || peers.rpcError;
			const body = E('div', { 'class': 'les-chat-stage' }, [
				E('p', { 'class': 'les-chat-section-title' }, [
					_('UDP announces in the last 20 seconds')
				]),
				renderList(status, peers, rpcError)
			]);
			L.dom.content(container, common.shell({
				title: _('Peers'),
				status: status,
				peerCount: Array.isArray(peers.peers) ? peers.peers.length : 0,
				rpcError: rpcError,
				body: body
			}));
		};

		paint(initialStatus, initialPeers);

		poll.add(function() {
			if (!document.getElementById('les-chat-peers-body'))
				return Promise.resolve();
			return Promise.all([
				callStatus().catch(function(error) { return { rpcError: error }; }),
				callPeers().catch(function(error) { return { rpcError: error, peers: [] }; })
			]).then(function(next) {
				paint(next[0] || {}, next[1] || {});
			});
		}, 5);

		return container;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
