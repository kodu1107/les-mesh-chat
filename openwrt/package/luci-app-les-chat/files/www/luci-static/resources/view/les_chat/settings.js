'use strict';
'require view';
'require form';
'require fs';
'require uci';
'require ui';
'require rpc';
'require view.les_chat.common as common';

const callStatus = rpc.declare({
	object: 'luci.leschat',
	method: 'status'
});

const callPeers = rpc.declare({
	object: 'luci.leschat',
	method: 'peers'
});

return view.extend({
	load: function() {
		common.loadCss();
		return Promise.all([
			callStatus().catch(function(error) { return { rpcError: error }; }),
			callPeers().catch(function() { return { peers: [] }; })
		]);
	},

	render: function(data) {
		const status = data[0] || {};
		const peers = data[1] || {};
		let m, s, o;

		m = new form.Map('les-chat', '', '');

		s = m.section(form.NamedSection, 'main', 'les_chatd', _('Service'));
		s.addremove = false;

		s.tab('general', _('General'));
		s.tab('network', _('Advanced network'));
		s.tab('storage', _('Storage'));

		o = s.taboption('general', form.Flag, 'enabled', _('Enabled'),
			_('Start les-chatd at boot and keep it running.'));
		o.rmempty = false;
		o.default = '1';

		o = s.taboption('general', form.Value, 'callsign', _('Callsign'),
			_('Name announced to peers. auto uses the device hostname.'));
		o.placeholder = 'auto';
		o.rmempty = false;

		o = s.taboption('general', form.Value, 'node_id', _('Node ID'),
			_('Stable identity for this node. Keep auto unless you are recovering a node. Do not copy this value between devices.'));
		o.placeholder = 'auto';
		o.rmempty = false;

		o = s.taboption('network', form.Value, 'bind', _('HTTP bind address'),
			_('0.0.0.0 serves all interfaces. Do not expose this on WAN.'));
		o.datatype = 'ip4addr';
		o.placeholder = '0.0.0.0';
		o.rmempty = false;

		o = s.taboption('network', form.Value, 'port', _('HTTP port'));
		o.datatype = 'port';
		o.placeholder = '7777';
		o.rmempty = false;

		o = s.taboption('network', form.Value, 'discovery_address', _('Discovery address'),
			_('UDP announce destination. Use 255.255.255.255 on the mesh.'));
		o.datatype = 'ip4addr';
		o.placeholder = '255.255.255.255';
		o.rmempty = false;

		o = s.taboption('network', form.Value, 'discovery_interface', _('Discovery interface'),
			_('auto selects br-ahwlan when present. MeshGate nodes with overlapping LAN/HaLow ranges should set br-ahwlan.'));
		o.placeholder = 'auto';
		o.rmempty = false;

		o = s.taboption('network', form.Value, 'discovery_port', _('Discovery port'));
		o.datatype = 'port';
		o.placeholder = '7777';
		o.rmempty = false;

		o = s.taboption('storage', form.Value, 'database', _('Database path'),
			_('SQLite file on overlay storage. Changing this path does not move existing messages.'));
		o.placeholder = '/overlay/les-chat/messages.db';
		o.rmempty = false;

		return m.render().then(function(formNode) {
			const note = E('p', { 'class': 'les-chat-settings-note' }, [
				_('General options are enough for most nodes. Network and storage tabs change how the daemon binds and where history is stored. Save & Apply restarts les-chatd.')
			]);
			return common.shell({
				title: _('Settings'),
				status: status,
				peerCount: Array.isArray(peers.peers) ? peers.peers.length : 0,
				rpcError: status.rpcError,
				body: E('div', {}, [ note, formNode ])
			});
		});
	},

	handleSaveApply: function(ev, mode) {
		ui.addNotification(null, E('p', {}, [
			_('Saving settings and restarting les-chatd…')
		]), 'info');
		return this.handleSave(ev).then(function() {
			return uci.save();
		}).then(function() {
			return fs.exec('/etc/init.d/les-chatd', [ 'restart' ]);
		}).then(function(result) {
			if (result && result.code && result.code !== 0)
				throw new Error(result.stderr || result.stdout || _('les-chatd restart failed'));
			ui.addNotification(null, E('p', {}, [
				_('les-chatd restarted.')
			]), 'info');
			return ui.changes.apply(mode == '0');
		}).catch(function(error) {
			ui.addNotification(null, E('p', {}, [ error.message || error ]), 'error');
			throw error;
		});
	}
});
