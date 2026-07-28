// Apple-Style UI Controller for RS485 Node
class RS485NodeUI {
    constructor() {
        this.updateInterval = null;
        this.init();
    }

    async init() {
        this.setupEventListeners();
        this.startLiveUpdates();
        await this.loadInitialData();
    }

    setupEventListeners() {
        window.addEventListener('click', (e) => {
            if (e.target.classList.contains('modal')) {
                closeModal();
            }
        });
    }

    async loadInitialData() {
        try {
            const status = await this.fetchAPI('/api/status');
            this.updateStatusBar(status);

            const data = await this.fetchAPI('/api/data');
            this.updateSensorSelector(data.sensor_type);
            this.updateSensors(data);

            const mqtt = await this.fetchAPI('/api/settings/mqtt');
            this.updateMQTTSettings(mqtt);
        } catch (error) {
            console.error('Failed to load initial data:', error);
            this.showError();
        }
    }

    startLiveUpdates() {
        if (this.updateInterval) clearInterval(this.updateInterval);

        this.updateInterval = setInterval(async () => {
            try {
                const [status, data] = await Promise.all([
                    this.fetchAPI('/api/status'),
                    this.fetchAPI('/api/data')
                ]);
                this.updateStatusBar(status);
                this.updateSensorSelector(data.sensor_type);
                this.updateSensors(data);
            } catch (error) {
                console.error('Live update failed:', error);
            }
        }, 2000);
    }

    async fetchAPI(endpoint) {
        const response = await fetch(endpoint);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return await response.json();
    }

    updateStatusBar(status) {
        const wifiIndicator = document.getElementById('wifiIndicator');
        const mqttIndicator = document.getElementById('mqttIndicator');
        const ipAddress = document.getElementById('ipAddress');

        if (wifiIndicator) {
            wifiIndicator.classList.toggle('active', !!status?.wifi?.connected);
            wifiIndicator.style.color = status?.wifi?.connected ? '#34c759' : '#ff3b30';
        }

        if (mqttIndicator) {
            mqttIndicator.classList.toggle('active', !!status?.mqtt?.connected);
            mqttIndicator.style.color = status?.mqtt?.connected ? '#34c759' : '#ff9500';
        }

        if (ipAddress) {
            const ip = status?.wifi?.ip;
            if (ip && ip.length) ipAddress.textContent = ip;
        }

        const version = document.getElementById('fwVersion');
        if (version) version.textContent = status?.fw || '';
    }

    // Maps each sensor type to its card-grid element id — add a new
    // entry here (plus a <select> option and card grid in index.html)
    // when a new sensor type is added to the firmware.
    static SENSOR_CARD_IDS = { th: 'thCards', co2: 'co2Cards', ec: 'ecCards', ph: 'phCards', npk: 'npkCards' };

    updateSensorSelector(type) {
        if (!type) return;
        currentSensorType = type;
        const select = document.getElementById('sensorSelect');
        if (select) select.value = type;
        for (const [t, cardId] of Object.entries(RS485NodeUI.SENSOR_CARD_IDS)) {
            const el = document.getElementById(cardId);
            if (el) el.style.display = (t === type) ? '' : 'none';
        }
    }

    setValueText(elId, value, decimals) {
        const el = document.getElementById(elId);
        if (!el) return;
        const ok = typeof value === 'number' && isFinite(value);
        el.textContent = ok ? value.toFixed(decimals) : (decimals === 0 ? '--' : '--.-');
    }

    setBadge(elId, ok) {
        const el = document.getElementById(elId);
        if (!el) return;
        el.textContent = ok ? '● Live' : '● No data';
        el.style.color = ok ? '#34c759' : '#ff9500';
    }

    updateSensors(data) {
        this.setValueText('tempValue', data?.temp_c, 1);
        this.setValueText('humValue', data?.humidity_percent, 1);
        this.setValueText('co2Value', data?.co2_ppm, 0);
        this.setValueText('co2TempValue', data?.co2_temp_c, 1);
        this.setValueText('co2HumValue', data?.co2_humidity_percent, 1);
        this.setValueText('ecValue', data?.ec_tds_value, 1);
        this.setValueText('phValue', data?.ph_value, 1);
        this.setValueText('phTempValue', data?.ph_temp_c, 1);
        this.setValueText('npkNValue', data?.npk_n, 0);
        this.setValueText('npkPValue', data?.npk_p, 0);
        this.setValueText('npkKValue', data?.npk_k, 0);

        this.setBadge('thBadge', !!data?.th_valid);
        this.setBadge('co2Badge', !!data?.co2_valid);
        this.setBadge('ecBadge', !!data?.ec_valid);
        this.setBadge('phBadge', !!data?.ph_valid);
        this.setBadge('npkBadge', !!data?.npk_valid);
    }

    updateMQTTSettings(config) {
        const provStatus = document.getElementById('provStatus');
        const provForm = document.getElementById('provForm');
        const reproveBtn = document.getElementById('reproveBtn');
        const statusText = document.getElementById('statusText');
        const statusDot = document.querySelector('#mqttConnectionStatus .status-dot');

        if (config.provisioned) {
            if (provStatus) {
                provStatus.className = 'prov-status ok';
                provStatus.textContent = `✓ Provisioned — topic: ${config.base_topic}`;
            }
            if (provForm) provForm.style.display = 'none';
            if (reproveBtn) reproveBtn.style.display = '';

            if (config.enabled && statusText) {
                statusText.textContent = config.connected ? 'Connected to Platform' : 'Connecting...';
                if (statusDot) {
                    statusDot.classList.toggle('offline', !config.connected);
                    statusDot.style.color = config.connected ? '#34c759' : '#ff9500';
                }
            }
        } else {
            if (provStatus) provStatus.className = 'prov-status';
            if (provForm) provForm.style.display = '';
            if (reproveBtn) reproveBtn.style.display = 'none';
        }
    }

    showError() {
        const container = document.querySelector('.main-content');
        if (container) {
            const error = document.createElement('div');
            error.className = 'error-toast';
            error.textContent = 'Unable to connect to device';
            error.style.cssText = `
                position: fixed;
                bottom: 100px;
                left: 20px;
                right: 20px;
                background: rgba(255, 59, 48, 0.9);
                color: white;
                padding: 12px 20px;
                border-radius: 12px;
                text-align: center;
                font-size: 14px;
                backdrop-filter: blur(10px);
                animation: slideUp 0.3s ease;
                z-index: 1000;
            `;
            container.appendChild(error);
            setTimeout(() => error.remove(), 3000);
        }
    }
}

let currentSensorType = 'th';

// Maps each sensor type to its offset-group element id and the input
// fields it owns — mirrors RS485NodeUI.SENSOR_CARD_IDS. Add an entry here
// (plus the matching offset-group markup in index.html and offset struct
// field in main.cpp) when a new sensor type is added.
const OFFSET_GROUPS = {
    th:  { groupId: 'offTh',  fields: { temp_c: 'offTempC', humidity_percent: 'offHumidity' } },
    co2: { groupId: 'offCo2', fields: { co2_ppm: 'offCo2Ppm', co2_temp_c: 'offCo2TempC', co2_humidity_percent: 'offCo2Humidity' } },
    ec:  { groupId: 'offEc',  fields: { ec_tds_value: 'offEcTds' } },
    ph:  { groupId: 'offPh',  fields: { ph_value: 'offPhValue', ph_temp_c: 'offPhTempC' } },
    npk: { groupId: 'offNpk', fields: { npk_n: 'offNpkN', npk_p: 'offNpkP', npk_k: 'offNpkK' } },
};

async function selectSensor(type) {
    ui.updateSensorSelector(type);
    try {
        const r = await fetch('/api/sensor', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ type })
        });
        const j = await r.json();
        if (!j.ok) throw new Error(j.err || 'failed');
        const data = await ui.fetchAPI('/api/data');
        ui.updateSensors(data);
    } catch (e) {
        console.error('Failed to switch sensor:', e);
    }
}

function navigateTo(view) {
    if (view === 'settings') {
        document.getElementById('settingsModal').classList.add('active');
    } else if (view === 'mqtt') {
        closeModal();
        document.getElementById('mqttModal').classList.add('active');
    } else if (view === 'offset') {
        closeModal();
        loadOffsets();
        document.getElementById('offsetModal').classList.add('active');
    }
}

function closeModal() {
    document.querySelectorAll('.modal').forEach(modal => modal.classList.remove('active'));
}

async function loadOffsets() {
    const msgEl = document.getElementById('offsetMsg');
    msgEl.textContent = '';
    msgEl.className = 'wizard-msg';
    try {
        const cfg = await ui.fetchAPI('/api/settings/offsets');
        Object.values(OFFSET_GROUPS).forEach(g => {
            document.getElementById(g.groupId).style.display = 'none';
        });
        const group = OFFSET_GROUPS[currentSensorType] || OFFSET_GROUPS.th;
        document.getElementById(group.groupId).style.display = '';
        for (const [key, elId] of Object.entries(group.fields)) {
            const el = document.getElementById(elId);
            if (el) el.value = cfg[key] ?? 0;
        }
    } catch (e) {
        msgEl.textContent = 'Failed to load offsets: ' + e.message;
        msgEl.className = 'wizard-msg err';
    }
}

async function saveOffsets() {
    const group = OFFSET_GROUPS[currentSensorType] || OFFSET_GROUPS.th;
    const msgEl = document.getElementById('offsetMsg');
    const btn = document.getElementById('offsetSaveBtn');
    const body = {};
    for (const [key, elId] of Object.entries(group.fields)) {
        const v = parseFloat(document.getElementById(elId).value);
        body[key] = isFinite(v) ? v : 0;
    }

    btn.disabled = true;
    btn.textContent = 'Saving…';
    try {
        const r = await fetch('/api/settings/offsets', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        const j = await r.json();
        if (!j.ok) throw new Error(j.err || 'failed');
        msgEl.textContent = '✓ Offset saved';
        msgEl.className = 'wizard-msg ok';
    } catch (e) {
        msgEl.textContent = 'Error: ' + e.message;
        msgEl.className = 'wizard-msg err';
    } finally {
        btn.disabled = false;
        btn.textContent = 'Save Offset';
    }
}

async function connectToPlatform() {
    const topic  = document.getElementById('provTopic').value.trim();
    const apiKey = document.getElementById('provApiKey').value.trim();
    const btn    = document.getElementById('provBtn');
    const msgEl  = document.getElementById('provMsg');

    if (!topic || !apiKey) {
        msgEl.textContent = 'Topic and API key are required.';
        msgEl.className = 'wizard-msg err';
        return;
    }

    btn.disabled = true;
    btn.textContent = 'Provisioning…';
    msgEl.textContent = '';
    msgEl.className = 'wizard-msg';

    try {
        const fd = new FormData();
        fd.append('topic', topic);
        fd.append('api_key', apiKey);

        const r = await fetch('/api/provision', { method: 'POST', body: fd });
        const j = await r.json();

        if (!j.ok) throw new Error(j.err || 'unknown error');

        msgEl.textContent = 'Provisioned! Connecting to platform…';
        msgEl.className = 'wizard-msg ok';

        let connected = false;
        for (let i = 0; i < 20; i++) {
            await new Promise(r => setTimeout(r, 500));
            try {
                const cfg = await ui.fetchAPI('/api/settings/mqtt');
                if (cfg.enabled && cfg.connected) {
                    connected = true;
                    break;
                }
            } catch(e) {}
        }

        if (connected) {
            msgEl.textContent = '✓ Connected to platform!';
            msgEl.className = 'wizard-msg ok';
            setTimeout(() => closeModal(), 2000);
        } else {
            msgEl.textContent = 'Provisioned but still connecting (may take a moment)';
            msgEl.className = 'wizard-msg ok';
        }

        ui.fetchAPI('/api/settings/mqtt').then(cfg => ui.updateMQTTSettings(cfg)).catch(() => {});
    } catch(e) {
        msgEl.textContent = 'Error: ' + e.message;
        msgEl.className = 'wizard-msg err';
        btn.disabled = false;
        btn.textContent = 'Connect';
    }
}

async function clearAndReprovision() {
    if (!confirm('Clear provisioning settings?')) return;
    try {
        const r = await fetch('/api/clear-provision', { method: 'POST' });
        const j = await r.json();
        if (j.ok) {
            location.reload();
        } else {
            alert('Failed: ' + (j.err || 'unknown error'));
        }
    } catch(e) {
        alert('Error: ' + e.message);
    }
}

const ui = new RS485NodeUI();
