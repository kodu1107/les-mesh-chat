'use strict';
'require view';
'require rpc';
'require poll';

const callStatus = rpc.declare({
	object: 'luci.leschat',
	method: 'status'
});

function formatBytes(bytes) {
	const value = Number(bytes);
	if (!isFinite(value) || value < 0)
		return _('unknown');
	if (value < 1024)
		return '%d B'.format(value);
	return '%.2f MiB'.format(value / (1024 * 1024));
}

function row(label, value) {
	return E('tr', { 'class': 'tr' }, [
		E('td', { 'class': 'td left', 'width': '33%' }, [ label ]),
		E('td', { 'class': 'td left' }, [
			(value != null && value !== '') ? value : '-'
		])
	]);
}

function renderStatus(data) {
	const status = data || {};
	const running = status.running === true || status.running === 1;
	const healthzOk = status.healthz_ok === true || status.healthz_ok === 1;
	const stateLabel = running ? _('Running') : _('Not running');
	const healthzLabel = healthzOk ? _('ok') : (status.error || _('unreachable'));

	return E('div', {}, [
		E('div', {
			'class': running ? 'alert-message success' : 'alert-message warning'
		}, [
			running
				? _('The LES Mesh Chat daemon is running.')
				: _('The LES Mesh Chat daemon is not running.')
		]),
		E('table', { 'class': 'table' }, [
			row(_('Daemon'), stateLabel),
			row(_('Health'), healthzLabel),
			row(_('App version'), status.version),
			row(_('Service'), status.service),
			row(_('Node ID'), status.node_id),
			row(_('Callsign'), status.callsign),
			row(_('HTTP bind'), status.bind_address),
			row(_('HTTP port'), status.port),
			row(_('Database path'), status.database),
			row(_('Database size'), formatBytes(status.database_bytes))
		])
	]);
}

return view.extend({
	render: function() {
		const container = E('div', { 'id': 'les-chat-status' }, [
			E('em', {}, [ _('Loading…') ])
		]);

		poll.add(L.bind(function() {
			return callStatus().then(L.bind(function(data) {
				L.dom.content(container, renderStatus(data));
			}, this)).catch(function(error) {
				L.dom.content(container, E('div', {
					'class': 'alert-message danger'
				}, [ _('Unable to read daemon status: %s').format(error.message || error) ]));
			});
		}, this), 5);

		return E('div', {}, [
			E('h2', {}, [ _('LES Mesh Chat Status') ]),
			E('div', { 'class': 'cbi-map-descr' }, [
				_('Local daemon health, identity, and database usage.')
			]),
			container
		]);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
