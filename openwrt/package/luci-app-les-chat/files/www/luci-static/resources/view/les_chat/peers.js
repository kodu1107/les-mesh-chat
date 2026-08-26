'use strict';
'require view';
'require rpc';
'require poll';

const callPeers = rpc.declare({
	object: 'luci.leschat',
	method: 'peers'
});

function formatLastSeen(ms) {
	const value = Number(ms);
	if (!isFinite(value) || value <= 0)
		return '-';

	const when = new Date(value);
	if (isNaN(when.getTime()))
		return '-';

	const delta = Date.now() - value;
	let relative = when.toLocaleString();
	if (delta >= 0 && delta < 60000)
		relative = _('%d s ago').format(Math.floor(delta / 1000));
	else if (delta >= 0 && delta < 3600000)
		relative = _('%d min ago').format(Math.floor(delta / 60000));

	return relative;
}

function renderPeers(data) {
	const result = data || {};
	const peers = Array.isArray(result.peers) ? result.peers : [];
	const running = result.running === true || result.running === 1;

	const header = E('div', {
		'class': running ? 'alert-message success' : 'alert-message warning'
	}, [
		running
			? _('Online peers: %d').format(peers.length)
			: _('The LES Mesh Chat daemon is not running.')
	]);

	if (!running && result.error)
		header.appendChild(E('div', {}, [ result.error ]));

	if (peers.length === 0) {
		return E('div', {}, [
			header,
			E('p', {}, [ _('No peers have announced within the last 20 seconds.') ])
		]);
	}

	const table = E('table', { 'class': 'table' }, [
		E('tr', { 'class': 'tr table-titles' }, [
			E('th', { 'class': 'th' }, [ _('Callsign') ]),
			E('th', { 'class': 'th' }, [ _('Node ID') ]),
			E('th', { 'class': 'th' }, [ _('IPv4 address') ]),
			E('th', { 'class': 'th' }, [ _('HTTP port') ]),
			E('th', { 'class': 'th' }, [ _('App version') ]),
			E('th', { 'class': 'th' }, [ _('Last seen') ])
		])
	]);

	peers.forEach(function(peer) {
		table.appendChild(E('tr', { 'class': 'tr' }, [
			E('td', { 'class': 'td' }, [ peer.callsign || '-' ]),
			E('td', { 'class': 'td' }, [ peer.node_id || '-' ]),
			E('td', { 'class': 'td' }, [ peer.address || '-' ]),
			E('td', { 'class': 'td' }, [ peer.http_port != null ? String(peer.http_port) : '-' ]),
			E('td', { 'class': 'td' }, [ peer.app_version || '-' ]),
			E('td', { 'class': 'td' }, [ formatLastSeen(peer.last_seen_ms) ])
		]));
	});

	return E('div', {}, [ header, table ]);
}

return view.extend({
	render: function() {
		const container = E('div', { 'id': 'les-chat-peers' }, [
			E('em', {}, [ _('Loading…') ])
		]);

		poll.add(L.bind(function() {
			return callPeers().then(function(data) {
				L.dom.content(container, renderPeers(data));
			}).catch(function(error) {
				L.dom.content(container, E('div', {
					'class': 'alert-message danger'
				}, [ _('Unable to read peers: %s').format(error.message || error) ]));
			});
		}, this), 5);

		return E('div', {}, [
			E('h2', {}, [ _('LES Mesh Chat Peers') ]),
			E('div', { 'class': 'cbi-map-descr' }, [
				_('Nodes discovered over UDP in the last 20 seconds.')
			]),
			container
		]);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
