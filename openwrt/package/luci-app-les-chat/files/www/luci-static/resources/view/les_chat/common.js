'use strict';
'require baseclass';

let cssLoaded = false;

function isTrue(value) {
	return value === true || value === 1 || value === '1';
}

return baseclass.extend({
	loadCss: function() {
		if (cssLoaded || document.getElementById('les-chat-stylesheet'))
			return;

		cssLoaded = true;
		document.head.appendChild(E('link', {
			id: 'les-chat-stylesheet',
			rel: 'stylesheet',
			type: 'text/css',
			href: L.resource('view/les_chat/les_chat.css')
		}));
	},

	isTrue: isTrue,

	connectionState: function(status, rpcError) {
		if (rpcError)
			return { id: 'reconnect', label: _('Reconnecting') };

		if (!status)
			return { id: 'loading', label: _('Checking') };

		if (isTrue(status.running) && isTrue(status.healthz_ok))
			return { id: 'online', label: _('Online') };

		if (isTrue(status.running))
			return { id: 'reconnect', label: _('Reconnecting') };

		return { id: 'stopped', label: _('Stopped') };
	},

	badge: function(state) {
		const info = state || { id: 'loading', label: _('Checking') };
		return E('div', {
			'class': 'les-chat-badge les-chat-badge-' + info.id,
			'role': 'status'
		}, [ info.label ]);
	},

	empty: function(title, detail) {
		return E('div', { 'class': 'les-chat-empty' }, [
			E('strong', {}, [ title ]),
			E('p', {}, [ detail ])
		]);
	},

	shell: function(opts) {
		const status = opts.status || {};
		const state = opts.state || this.connectionState(status, opts.rpcError);
		const peerCount = opts.peerCount;
		const metrics = [
			E('div', {}, [
				E('dt', {}, [ _('Peers') ]),
				E('dd', {}, [
					peerCount == null ? '—' : String(peerCount)
				])
			]),
			E('div', {}, [
				E('dt', {}, [ _('Daemon') ]),
				E('dd', {}, [ state.label ])
			])
		];

		if (opts.extraMetrics)
			opts.extraMetrics.forEach(function(item) {
				metrics.push(item);
			});

		return E('div', { 'class': 'les-chat-app' }, [
			E('div', { 'class': 'les-chat-panel' }, [
				E('div', { 'class': 'les-chat-topbar' }, [
					E('div', {}, [
						E('p', { 'class': 'les-chat-eyebrow' }, [
							_('OpenMANET communications')
						]),
						E('h2', { 'class': 'les-chat-title' }, [
							opts.title || _('LES Mesh Chat')
						])
					]),
					this.badge(state)
				]),
				E('div', { 'class': 'les-chat-identity' }, [
					E('div', { 'class': 'les-chat-id-block' }, [
						E('p', { 'class': 'les-chat-callsign' }, [
							status.callsign || '—'
						]),
						E('p', { 'class': 'les-chat-node-id' }, [
							_('Node ID') + ': ' + (status.node_id || '—')
						])
					]),
					E('dl', { 'class': 'les-chat-metrics' }, metrics)
				]),
				opts.body
			])
		]);
	},

	formatLastSeen: function(ms) {
		const value = Number(ms);
		if (!isFinite(value) || value <= 0)
			return '—';

		const when = new Date(value);
		if (isNaN(when.getTime()))
			return '—';

		const delta = Date.now() - value;
		if (delta >= 0 && delta < 60000)
			return _('%d s ago').format(Math.floor(delta / 1000));
		if (delta >= 0 && delta < 3600000)
			return _('%d min ago').format(Math.floor(delta / 60000));
		return when.toLocaleString();
	},

	formatBytes: function(bytes) {
		const value = Number(bytes);
		if (!isFinite(value) || value < 0)
			return _('unknown');
		if (value < 1024)
			return '%d B'.format(value);
		return '%.2f MiB'.format(value / (1024 * 1024));
	},

	formatTime: function(ms) {
		const date = new Date(ms);
		if (Number.isNaN(date.getTime()))
			return '';
		return date.toLocaleTimeString();
	}
});
