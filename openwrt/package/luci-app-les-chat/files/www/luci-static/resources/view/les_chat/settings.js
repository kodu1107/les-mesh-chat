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
		s.tab('time', _('Mesh time'));
		s.tab('storage', _('Storage'));

		o = s.taboption('general', form.Flag, 'enabled', _('Enabled'),
			_('Start les-chatd at boot and keep it running.'));
		o.rmempty = false;
		o.default = '1';

		o = s.taboption('general', form.Value, 'callsign', _('Nickname (required)'),
			_('The name announced to peers. Node ID is generated once and restored automatically after updates.'));
		o.placeholder = _('Enter a nickname');
		o.rmempty = false;
		o.validate = function(section_id, value) {
			if (!value || value === 'auto' || !value.trim())
				return _('Enter a nickname before saving.');
			if (value.length > 64)
				return _('Nickname cannot exceed 64 bytes.');
			return true;
		};

		o = s.taboption('general', form.Value, 'node_id', _('Node ID'),
			_('Use auto to keep the generated stable ID, or enter a unique manual ID (1–64 letters, numbers, ., _ or -). Changing it creates a new message identity.'));
		o.placeholder = 'auto';
		o.default = 'auto';
		o.rmempty = false;
		o.validate = function(section_id, value) {
			value = String(value || '');
			if (!value)
				return _('Enter auto or a unique Node ID.');
			if (value !== value.trim())
				return _('Node ID cannot start or end with spaces.');
			if (value === 'auto')
				return true;
			if (value.length > 64)
				return _('Node ID cannot exceed 64 characters.');
			if (!/^[A-Za-z0-9._-]+$/.test(value))
				return _('Node ID may contain only letters, numbers, period, underscore, and hyphen.');
			return true;
		};

		o = s.taboption('general', form.DummyValue, '_effective_node_id', _('Current effective Node ID'),
			_('The ID currently announced by les-chatd.'));
		o.cfgvalue = function() {
			return status.node_id || _('Generated on first boot');
		};

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

		o = s.taboption('time', form.ListValue, 'time_sync_mode', _('Time sync mode'),
			_('MeshGate authority time keeps message timestamps aligned without Internet access.'));
		o.value('client', _('Point / client'));
		o.value('authority', _('MeshGate authority'));
		o.value('off', _('Disabled'));
		o.default = 'client';
		o.rmempty = false;

		o = s.taboption('time', form.Value, 'time_authority_id', _('Trusted authority Node ID'),
			_('Optional. Leave empty to accept the first announced MeshGate authority.'));
		o.placeholder = 'node-bolt';


		o = s.taboption('storage', form.Value, 'database', _('Database path'),
			_('SQLite file on overlay storage. Changing this path does not move existing messages.'));
		o.placeholder = '/overlay/les-chat/messages.db';
		o.rmempty = false;

		return m.render().then(function(formNode) {
			const note = E('p', { 'class': 'les-chat-settings-note' }, [
				_('Set the nickname once. Keep Node ID as auto for a generated persistent ID, or enter a unique manual ID. Save & Apply restarts les-chatd.')
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
