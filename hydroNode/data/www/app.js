// Apple-Style UI Controller for HydroNode
class HydroNodeUI {
    constructor() {
        this.apiBase = '';
        this.updateInterval = null;
        this.currentView = 'home';
        this.init();
    }

    async init() {
        this.setupEventListeners();
        this.startLiveUpdates();
        await this.loadInitialData();
        this.animateEntrance();
    }

    setupEventListeners() {
        // Smooth scrolling for iOS feel
        document.querySelectorAll('a[href^="#"]').forEach(anchor => {
            anchor.addEventListener('click', function (e) {
                e.preventDefault();
                document.querySelector(this.getAttribute('href')).scrollIntoView({
                    behavior: 'smooth'
                });
            });
        });

        // Modal close on background click
        window.addEventListener('click', (e) => {
            if (e.target.classList.contains('modal')) {
                closeModal();
            }
        });
    }

    animateEntrance() {
        const elements = document.querySelectorAll('.sensor-card, .control-center, .hero');
        elements.forEach((el, index) => {
            el.style.animationDelay = `${index * 0.1}s`;
        });
    }

    async loadInitialData() {
        try {
            const status = await this.fetchAPI('/api/status');
            this.updateStatusBar(status);
            this.updateTempFromStatus(status);

            const ec = await this.fetchAPI('/api/ec');
            this.updateEC(ec);

            const level = await this.fetchAPI('/api/level');
            this.updateLevel(level);

            const cal = await this.fetchAPI('/api/cal');
            this.updateCalibrationStatus(cal);

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
                // ✅ get status too (temp + wifi/mqtt state)
                const [status, ec, level] = await Promise.all([
                    this.fetchAPI('/api/status'),
                    this.fetchAPI('/api/ec'),
                    this.fetchAPI('/api/level')
                ]);

                this.updateStatusBar(status);
                this.updateTempFromStatus(status);

                this.updateEC(ec);
                this.updateLevel(level);

                this.animateValueChange('ecValue');
                this.animateValueChange('levelValue');
                this.animateValueChange('tempValue');
            } catch (error) {
                console.error('Live update failed:', error);
            }
        }, 1000);
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

    // ✅ DS18B20 from /api/status => status.temp_c
    updateTempFromStatus(status) {
        const tempEl = document.getElementById('tempValue');
        if (!tempEl) return;

        const t = status?.temp_c;
        const ok = (typeof t === 'number') && isFinite(t);

        tempEl.textContent = ok ? t.toFixed(1) : '--.-';

        const badge = document.getElementById('tempBadge');
        if (badge) {
            badge.textContent = ok ? '● Live' : '● No data';
            badge.style.color = ok ? '#34c759' : '#ff9500';
        }
    }

    updateEC(data) {
        const ecValue = document.getElementById('ecValue');
        const ecVoltage = document.getElementById('ecVoltage');

        const us = Number(data?.us_cm);
        const ok = Number.isFinite(us);

        const ms = ok ? us / 1000.0 : NaN;

        if (ecValue) {
            ecValue.textContent = ok ? ms.toFixed(2) : '--.--';
        }

        if (ecVoltage) {
            ecVoltage.textContent = Number.isFinite(data?.v)
                ? `${data.v.toFixed(3)}V`
                : '--.-V';
        }
    }


    updateLevel(data) {
        const levelValue = document.getElementById('levelValue');
        const levelVoltage = document.getElementById('levelVoltage');
        const waterProgress = document.getElementById('waterProgress');
        const levelUnit = document.getElementById('levelUnit');

        if (levelValue) {
            levelValue.textContent = data.percent.toFixed(1);
            this.animateProgress(waterProgress, data.percent);
        }

        if (levelVoltage) levelVoltage.textContent = `${data.v.toFixed(3)}V`;
        if (levelUnit) levelUnit.textContent = '%';
    }

    updateCalibrationStatus(cal) {
        const ecBadge = document.getElementById('ecCalBadge');
        const levelBadge = document.getElementById('levelCalBadge');
        const ecStatus = document.getElementById('ecCalStatus');
        const levelStatus = document.getElementById('levelCalStatus');

        if (ecBadge) {
            ecBadge.textContent = cal.ec.valid ? '✓ Calibrated' : '⚠ Needs Calibration';
            ecBadge.style.color = cal.ec.valid ? '#34c759' : '#ff9500';
        }

        if (levelBadge) {
            levelBadge.textContent = cal.level.valid ? '✓ Calibrated' : '⚠ Needs Calibration';
            levelBadge.style.color = cal.level.valid ? '#34c759' : '#ff9500';
        }

        if (ecStatus) {
            ecStatus.textContent = cal.ec.valid ?
                `${cal.ec.A_ec}µS / ${cal.ec.B_ec}µS` :
                '2-point calibration required';
        }

        if (levelStatus) {
            levelStatus.textContent = cal.level.valid ?
                `${cal.level.E_lvl}% → ${cal.level.F_lvl}%` :
                'Set empty & full points';
        }
    }

    updateMQTTSettings(config) {
        const enabled = document.getElementById('mqttEnabled');
        const host = document.getElementById('mqttHost');
        const port = document.getElementById('mqttPort');
        const user = document.getElementById('mqttUser');
        const pass = document.getElementById('mqttPass');
        const topic = document.getElementById('mqttTopic');
        const interval = document.getElementById('mqttInterval');

        if (enabled) enabled.checked = config.enabled;
        if (host) host.value = config.host || '';
        if (port) port.value = config.port || 1883;
        if (user) user.value = config.user || '';
        if (pass) pass.value = config.pass || '';
        if (topic) topic.value = config.base_topic || 'hydronode';
        if (interval) interval.value = config.pub_period_ms || 1000;

        this.toggleMQTTSettings(config.enabled);
    }

    toggleMQTTSettings(show) {
        const settings = document.getElementById('mqttSettings');
        if (settings) settings.style.display = show ? 'block' : 'none';
    }

    animateValueChange(elementId) {
        const el = document.getElementById(elementId);
        if (!el) return;
        el.classList.add('value-update');
        setTimeout(() => el.classList.remove('value-update'), 200);
    }

    animateProgress(progressBar, targetPercent) {
        if (!progressBar) return;
        const currentWidth = parseInt(progressBar.style.width) || 0;
        if (Math.abs(currentWidth - targetPercent) > 1) {
            progressBar.style.width = `${targetPercent}%`;
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

// Navigation
function navigateTo(view) {
    document.querySelectorAll('.nav-item').forEach(item => item.classList.remove('active'));

    const activeNav = Array.from(document.querySelectorAll('.nav-item')).find(
        item => item.querySelector('.nav-label').textContent.toLowerCase() ===
        (view === 'home' ? 'home' :
         view === 'calibration' ? 'cal' :
         view === 'mqtt' ? 'mqtt' : 'settings')
    );

    if (activeNav) activeNav.classList.add('active');

    if (view === 'calibration') {
        document.getElementById('calibrationModal').classList.add('active');
    } else if (view === 'mqtt') {
        document.getElementById('mqttModal').classList.add('active');
    }
}

function closeModal() {
    document.querySelectorAll('.modal').forEach(modal => modal.classList.remove('active'));
}

// Calibration functions
async function startECCalibration() {
    closeModal();
    alert('EC Calibration Wizard\n\n1. Place probe in 1413µS solution\n2. Press capture\n3. Place in 27600µS solution\n4. Press capture');
}

async function startLevelCalibration() {
    closeModal();
    alert('Level Calibration Wizard\n\n1. Set probe at empty level\n2. Press capture\n3. Set probe at full level\n4. Press capture');
}

// MQTT functions
function toggleMQTT() {
    const enabled = document.getElementById('mqttEnabled').checked;
    ui.toggleMQTTSettings(enabled);
}

async function saveMQTT() {
    const config = {
        enabled: document.getElementById('mqttEnabled').checked,
        host: document.getElementById('mqttHost').value,
        port: parseInt(document.getElementById('mqttPort').value),
        user: document.getElementById('mqttUser').value,
        pass: document.getElementById('mqttPass').value,
        base_topic: document.getElementById('mqttTopic').value,
        pub_period_ms: parseInt(document.getElementById('mqttInterval').value)
    };

    try {
        const response = await fetch('/api/settings/mqtt', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });

        if (response.ok) {
            const status = document.getElementById('mqttConnectionStatus');
            if (status) {
                status.innerHTML = '<span class="status-dot online"></span> Settings Saved ✓';
                status.style.color = '#34c759';
            }
            setTimeout(() => closeModal(), 1500);
        }
    } catch (error) {
        alert('Failed to save MQTT settings');
    }
}

// Initialize UI
const ui = new HydroNodeUI();
