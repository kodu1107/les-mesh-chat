'use strict';
'require view';
'require rpc';
'require poll';
'require uci';
'require view.les_chat.common as common';

const callStatus = rpc.declare({
	object: 'luci.leschat',
	method: 'status'
});

const callPeers = rpc.declare({
	object: 'luci.leschat',
	method: 'peers'
});

function kv(rows) {
	const table = E('table', { 'class': 'les-chat-kv' });
	rows.forEach(function(row) {
		table.appendChild(E('tr', {}, [
			E('th', {}, [ row[0] ]),
			E('td', {}, [ row[1] != null && row[1] !== '' ? row[1] : '—' ])
		]));
	});
	return table;
}

function renderBody(status, rpcError) {
	const running = common.isTrue(status.running);
	const healthzOk = common.isTrue(status.healthz_ok);
	let banner;

	if (rpcError)
		banner = common.empty(
			_('Cannot read daemon status'),
			_('The LuCI helper could not reach les-chatd. UCI values below are still shown.')
		);
	else if (!running)
		banner = common.empty(
			_('Chat service is stopped'),
			_('Start les-chatd from Settings. Messages and peer discovery stay down until it runs.')
		);
	else if (!healthzOk)
		banner = common.empty(
			_('Service is reconnecting'),
			_('The process is present but /healthz did not answer. Wait and refresh.')
		);

	return E('div', { 'class': 'les-chat-scroll' }, [
		banner || '',
		E('p', { 'class': 'les-chat-section-title' }, [ _('Identity') ]),
		kv([
			[ _('Callsign'), status.callsign ],
			[ _('Node ID'), status.node_id ],
			[ _('App version'), status.version ],
			[ _('Service'), status.service ]
		]),
		E('p', { 'class': 'les-chat-section-title' }, [ _('Daemon') ]),
		kv([
			[ _('Process'), running ? _('Running') : _('Not running') ],
			[ _('Health'), healthzOk ? _('ok') : (status.error || _('unreachable')) ],
			[ _('HTTP bind'), status.bind_address ],
			[ _('HTTP port'), status.port ]
		]),
		E('p', { 'class': 'les-chat-section-title' }, [ _('Discovery') ]),
		kv([
			[ _('Discovery address'), uci.get('les-chat', 'main', 'discovery_address') ],
			[ _('Discovery interface'), uci.get('les-chat', 'main', 'discovery_interface') ],
			[ _('Discovery port'), uci.get('les-chat', 'main', 'discovery_port') ]
		]),
		E('p', { 'class': 'les-chat-section-title' }, [ _('Storage') ]),
		kv([
			[ _('Database path'), status.database || uci.get('les-chat', 'main', 'database') ],
			[ _('Database size'), common.formatBytes(status.database_bytes) ]
		])
	]);
}

return view.extend({
	load: function() {
		common.loadCss();
		return Promise.all([
			uci.load('les-chat'),
			callStatus().catch(function(error) { return { rpcError: error }; }),
			callPeers().catch(function() { return { peers: [] }; })
		]);
	},

	render: function(data) {
		const container = E('div', { 'id': 'les-chat-status-body' });

		const paint = function(status, peers) {
			L.dom.content(container, common.shell({
				title: _('Status'),
				status: status,
				peerCount: Array.isArray(peers.peers) ? peers.peers.length : 0,
				rpcError: status.rpcError,
				body: renderBody(status, status.rpcError)
			}));
		};

		paint(data[1] || {}, data[2] || {});

		poll.add(function() {
			if (!document.getElementById('les-chat-status-body'))
				return Promise.resolve();
			return Promise.all([
				callStatus().catch(function(error) { return { rpcError: error }; }),
				callPeers().catch(function() { return { peers: [] }; })
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
