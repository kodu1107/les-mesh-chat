'use strict';
'require view';
'require form';
'require fs';
'require uci';
'require ui';

return view.extend({
	render: function() {
		let m, s, o;

		m = new form.Map('les-chat', _('LES Mesh Chat Settings'),
			_('These values are stored in UCI les-chat.main. Saving restarts the daemon.'));

		s = m.section(form.NamedSection, 'main', 'les_chatd', _('Service'));
		s.addremove = false;

		o = s.option(form.Flag, 'enabled', _('Enabled'),
			_('Start les-chatd at boot and keep it running.'));
		o.rmempty = false;
		o.default = '1';

		o = s.option(form.Value, 'node_id', _('Node ID'),
			_('Stable identity for this node. Keep auto unless you are recovering a node. Do not copy this value between devices.'));
		o.placeholder = 'auto';
		o.rmempty = false;

		o = s.option(form.Value, 'callsign', _('Callsign'),
			_('Display name announced to peers. auto uses the device hostname.'));
		o.placeholder = 'auto';
		o.rmempty = false;

		o = s.option(form.Value, 'bind', _('HTTP bind address'),
			_('Listen address for the chat API and web UI. 0.0.0.0 serves all interfaces; do not expose this on WAN.'));
		o.datatype = 'ip4addr';
		o.placeholder = '0.0.0.0';
		o.rmempty = false;

		o = s.option(form.Value, 'port', _('HTTP port'));
		o.datatype = 'port';
		o.placeholder = '7777';
		o.rmempty = false;

		o = s.option(form.Value, 'discovery_address', _('Discovery address'),
			_('UDP announce destination. Use 255.255.255.255 on the mesh.'));
		o.datatype = 'ip4addr';
		o.placeholder = '255.255.255.255';
		o.rmempty = false;

		o = s.option(form.Value, 'discovery_interface', _('Discovery interface'),
			_('auto selects br-ahwlan when present. MeshGate nodes with overlapping LAN/HaLow ranges should set br-ahwlan.'));
		o.placeholder = 'auto';
		o.rmempty = false;

		o = s.option(form.Value, 'discovery_port', _('Discovery port'));
		o.datatype = 'port';
		o.placeholder = '7777';
		o.rmempty = false;

		o = s.option(form.Value, 'database', _('Database path'),
			_('SQLite file on overlay storage. Changing this path does not move existing messages.'));
		o.placeholder = '/overlay/les-chat/messages.db';
		o.rmempty = false;

		return m.render();
	},

	handleSaveApply: function(ev, mode) {
		return this.handleSave(ev).then(function() {
			return uci.save();
		}).then(function() {
			return fs.exec('/etc/init.d/les-chatd', [ 'restart' ]);
		}).then(function(result) {
			if (result && result.code && result.code !== 0)
				throw new Error(result.stderr || result.stdout || _('les-chatd restart failed'));
			return ui.changes.apply(mode == '0');
		}).catch(function(error) {
			ui.addNotification(null, E('p', {}, [ error.message || error ]), 'error');
			throw error;
		});
	}
});
