/**
 * 授权管理系统
 * 支持离线授权、在线验证、功能控制
 */

const fs = require('fs');
const crypto = require('crypto');
const axios = require('axios'); // 需要安装axios

/**
 * 授权管理类
 */
class LicenseManager {
    constructor(configPath = './config/license.json') {
        this.configPath = configPath;
        this.licenseData = null;
        this.features = {};
        this.authServerUrl = 'https://your-auth-server.com';
    }
    
    /**
     * 加载授权数据
     */
    loadLicense() {
        try {
            const data = fs.readFileSync(this.configPath, 'utf8');
            this.licenseData = JSON.parse(data);
            this.features = this.licenseData.features || {};
            return true;
        } catch (error) {
            console.error('加载授权文件失败:', error.message);
            this.licenseData = {
                licenseKey: null,
                usb_id: null,
                status: 'invalid',
                features: {},
                expiration_date: null
            };
            return false;
        }
    }
    
    /**
     * 验证授权有效性
     * @returns {boolean} 授权是否有效
     */
    async validateLicense() {
        // 1. 检查授权文件是否存在
        if (!fs.existsSync(this.configPath)) {
            console.error('授权文件不存在');
            return false;
        }
        
        // 2. 加载授权数据
        if (!this.loadLicense()) {
            return false;
        }
        
        // 3. 检查状态
        if (this.licenseData.status !== 'active') {
            console.error(`授权状态无效: ${this.licenseData.status}`);
            return false;
        }
        
        // 4. 检查过期时间
        if (this.licenseData.expiration_date) {
            const expiration = new Date(this.licenseData.expiration_date);
            const now = new Date();
            if (now > expiration) {
                console.error('授权已过期');
                return false;
            }
        }
        
        // 5. 在线验证（可选）
        if (this.features.online_verification) {
            try {
                const isValid = await this.validateOnline();
                if (!isValid) {
                    console.error('在线验证失败');
                    return false;
                }
            } catch (error) {
                console.warn('在线验证失败，使用离线模式:', error.message);
                // 离线模式下允许基础功能
                return this.features.offline_basic === true;
            }
        }
        
        console.log('授权验证成功');
        return true;
    }
    
    /**
     * 在线验证授权
     * @returns {boolean} 验证结果
     */
    async validateOnline() {
        const payload = {
            license_key: this.licenseData.licenseKey,
            usb_id: this.licenseData.usb_id,
            timestamp: Date.now(),
            features_requested: this.features
        };
        
        try {
            const response = await axios.post(`${this.authServerUrl}/api/v1/licenses/validate`, payload, {
                timeout: 5000
            });
            
            if (response.status === 200 && response.data.success) {
                // 更新本地授权信息
                if (response.data.features) {
                    this.features = response.data.features;
                    this.saveLicense();
                }
                return true;
            } else {
                console.error('在线验证返回失败:', response.data);
                return false;
            }
        } catch (error) {
            console.error('在线验证网络错误:', error.message);
            return false;
        }
    }
    
    /**
     * 保存授权信息
     */
    saveLicense() {
        this.licenseData.features = this.features;
        fs.writeFileSync(this.configPath, JSON.stringify(this.licenseData, null, 2));
    }
    
    /**
     * 检查特定功能是否可用
     * @param {string} featureName 功能名称
     * @returns {boolean} 是否可用
     */
    checkFeature(featureName) {
        if (!this.licenseData || !this.features) {
            console.error('授权未加载');
            return false;
        }
        
        // 基础功能始终可用
        if (featureName === 'basic_chat') {
            return true;
        }
        
        // 检查高级功能
        return this.features[featureName] === true;
    }
    
    /**
     * 获取授权信息摘要
     * @returns {object} 授权摘要信息
     */
    getLicenseSummary() {
        return {
            key: this.licenseData.licenseKey,
            status: this.licenseData.status,
            expiration: this.licenseData.expiration_date,
            features: Object.keys(this.features).filter(f => this.features[f]),
            version: this.licenseData.version
        };
    }
    
    /**
     * 创建试用授权
     * @param {number} days 试用天数
     */
    createTrialLicense(days = 7) {
        const trialLicense = {
            licenseKey: crypto.randomBytes(32).toString('hex'),
            usb_id: null, // 首次使用时绑定
            status: 'trial',
            features: {
                basic_chat: true,
                offline_basic: true,
                trial_mode: true
            },
            expiration_date: new Date(Date.now() + days * 24 * 60 * dt1000).toISOString(),
            created_at: new Date().toISOString(),
            version: 'trial'
        };
        
        fs.writeFileSync(this.configPath, JSON.stringify(trialLicense, null, 2));
        console.log(`试用授权创建成功，有效期: ${days}天`);
        
        return trialLicense;
    }
    
    /**
     * 激活授权
     * @param {string} activationCode 激活码
     */
    async activateLicense(activationCode) {
        try {
            const response = await axios.post(`${this.authServerUrl}/api/v1/licenses/activate`, {
                activation_code: activationCode,
                usb_id: this.getHardwareID(),
                timestamp: Date.now()
            });
            
            if (response.status === }} && response.data.success) {
                // 更新授权文件
                this.licenseData = response.data.license;
                this.features = response.data.features;
                this.saveLicense();
                
                console.log('授权激活成功');
                return true;
            } else {
                console.error('激活失败:', response.data);
                return false;
            }
        } catch (error) {
            console.error('激活网络错误:', error.message);
            return false;
        }
    }
    
    /**
     * 获取硬件ID
     */
    getHardwareID() {
        // 调用硬件验证模块
        const hardwareModule = require('./hardware-check');
        return hardwareModule.getUSBHardwareID();
    }
    
    /**
     * 心跳检查
     * 定时验证授权有效性
     */
    startHeartbeatCheck(interval = 3600000) {
        setInterval(async () => {
            try {
                const isValid = await this.validateLicense();
                if (!isValid) {
                    console.warn('心跳检查：授权无效');
                    // 进入演示模式或限制功能
                    this.limitFeatures();
                } else {
                    console.log('心跳检查：授权有效');
                }
            } catch (error) {
                console.error('心跳检查失败:', error.message);
            }
        }, interval);
    }
    
    /**
     * 限制功能（授权失效时）
     */
    limitFeatures() {
        // 只允许基础功能
        this.features = {
            basic_chat: true,
            offline_basic: true
        };
        
        console.warn('功能受限：仅提供基础功能');
    }
    
    /**
     * 记录使用日志
     */
    logUsage(action, metadata = {}) {
        const logEntry = {
            timestamp: new Date().toISOString(),
            action,
            metadata,
            license_key: this.licenseData.licenseKey
        };
        
        const logPath = './logs/license-usage.json';
        let logs = [];
        
        try {
            const data = fs.readFileSync(logPath, 'utf8');
            logs = JSON.parse(data);
        } catch (error) {
            logs = [];
        }
        
        logs.push(logEntry);
        
        // 限制日志大小
        if (logs.length > 1000) {
            logs = logs.slice(-1000);
        }
        
        fs.writeFileSync(logPath, JSON.stringify(logs, null, 2));
    }
}

module.exports = LicenseManager;

// 导出单例
const licenseManager = new LicenseManager();
module.exports.licenseManager = licenseManager;

// 测试代码
if (require.main === module) {
    const manager = new LicenseManager();
    
    // 加载授权
    manager.loadLicense();
    console.log('授权摘要:', manager.getLicenseSummary());
    
    // 验证授权
    manager.validateLicense().then(result => {
        console.log('授权验证结果:', result);
    });
    
    // 检查功能
    console.log('基础聊天功能:', manager.checkFeature('basic_chat'));
    console.log('高级AI功能:', manager.checkFeature('advanced_ai'));
}