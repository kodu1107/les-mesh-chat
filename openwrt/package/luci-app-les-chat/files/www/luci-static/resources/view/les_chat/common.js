'use strict';
'require baseclass';
'require rpc';
'require ui';

let cssLoaded = false;
const themeStorageKey = 'les-chat-theme';

const callRestart = rpc.declare({
	object: 'luci.leschat',
	method: 'restart'
});

function isTrue(value) {
	return value === true || value === 1 || value === '1';
}

return baseclass.extend({
	themeMode: function() {
		try {
			return localStorage.getItem(themeStorageKey) === 'dark' ? 'dark' : 'light';
		}
		catch (error) {
			return 'light';
		}
	},

	storeThemeMode: function(mode) {
		try {
			localStorage.setItem(themeStorageKey, mode);
		}
		catch (error) {
			/* Private browsing or restricted storage should not break the UI. */
		}
	},

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

		const themeMode = this.themeMode();
		const themeToggle = E('button', {
			'class': 'les-chat-theme-toggle',
			'type': 'button',
			'aria-pressed': themeMode === 'dark' ? 'true' : 'false',
			'title': themeMode === 'dark' ? _('Switch to day mode') : _('Switch to night mode')
		}, [ themeMode === 'dark' ? _('Day mode') : _('Night mode') ]);
		const restartButton = state.id === 'stopped' || state.id === 'reconnect' ? E('button', {
			'class': 'les-chat-restart',
			'type': 'button',
			'title': state.id === 'stopped' ? _('Start les-chatd') : _('Reconnect les-chatd')
		}, [ state.id === 'stopped' ? _('Start service') : _('Reconnect') ]) : null;
		const root = E('div', {
			'class': 'les-chat-app',
			'data-les-theme': themeMode
		}, [
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
					E('div', { 'class': 'les-chat-topbar-actions' }, [
						restartButton || '',
						themeToggle,
						this.badge(state)
					])
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
				!isTrue(status.callsign_configured) ? E('div', {
					'class': 'les-chat-identity-alert',
					'role': 'alert'
				}, [
					E('strong', {}, [ _('Nickname is not set') ]),
					E('span', {}, [ _('Set a nickname in Settings so other nodes can identify this device.') ]),
					E('a', {
						'href': L.url('admin/services/les-chat/settings')
					}, [ _('Open Settings') ])
				]) : '',
				opts.body
			])
		]);

		themeToggle.addEventListener('click', L.bind(function() {
			const nextMode = root.getAttribute('data-les-theme') === 'dark' ? 'light' : 'dark';
			root.setAttribute('data-les-theme', nextMode);
			themeToggle.setAttribute('aria-pressed', nextMode === 'dark' ? 'true' : 'false');
			themeToggle.setAttribute('title', nextMode === 'dark' ? _('Switch to day mode') : _('Switch to night mode'));
			themeToggle.textContent = nextMode === 'dark' ? _('Day mode') : _('Night mode');
			this.storeThemeMode(nextMode);
		}, this));

		if (restartButton) {
			restartButton.addEventListener('click', function() {
				restartButton.disabled = true;
				restartButton.textContent = _('Starting…');
				callRestart().then(function(result) {
					if (!result || (result.ok !== true && result.ok !== 1 && result.ok !== '1'))
						throw new Error(_('les-chatd restart failed'));
					setTimeout(function() {
						if (document.body.contains(root))
							location.reload();
					}, 1200);
				}).catch(function(error) {
					restartButton.disabled = false;
					restartButton.textContent = state.id === 'stopped' ? _('Start service') : _('Reconnect');
					if (window.ui && ui.addNotification)
						ui.addNotification(null, E('p', {}, [ error.message || error ]), 'error');
				});
			});
		}

		return root;
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
