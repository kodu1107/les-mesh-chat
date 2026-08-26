'use strict';
'require view';
'require uci';

function chatPort() {
	const port = parseInt(uci.get('les-chat', 'main', 'port'), 10);
	if (!isFinite(port) || port < 1 || port > 65535)
		return 7777;
	return port;
}

function chatUrl() {
	const host = window.location.hostname || '127.0.0.1';
	return 'http://' + host + ':' + chatPort() + '/';
}

return view.extend({
	load: function() {
		return uci.load('les-chat');
	},

	render: function() {
		const url = chatUrl();

		return E('div', {}, [
			E('h2', {}, [ _('LES Mesh Chat') ]),
			E('div', { 'class': 'cbi-map-descr' }, [
				_('Opens the existing chat UI on this node. The page uses the device hostname and the configured HTTP port, not 127.0.0.1.')
			]),
			E('p', {}, [
				E('a', {
					'href': url,
					'target': '_blank',
					'rel': 'noopener noreferrer',
					'class': 'btn cbi-button cbi-button-action'
				}, [ _('Open chat in a new tab') ]),
				' ',
				E('span', { 'class': 'cbi-value-description' }, [ url ])
			]),
			E('iframe', {
				'src': url,
				'title': 'LES Mesh Chat',
				'style': 'width:100%;height:70vh;border:1px solid #555;background:#000'
			})
		]);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
