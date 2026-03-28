/**
 * 硬件绑定验证模块
 * 用于验证U盘硬件ID，防止软件被复制到其他设备
 */

const fs = require('fs');
const crypto = require('crypto');
const { execSync } = require('child_process');

/**
 * 获取USB设备硬件ID
 * @returns {string} 硬件ID的SHA256哈希值
 */
function getUSBHardwareID() {
    try {
        // Windows平台：通过WMIC获取USB序列号
        if (process.platform === 'win32') {
            const cmd = 'wmic diskdrive get SerialNumber /value';
            const output = execSync(cmd, { encoding: 'utf8' });
            const lines = output.split('\n');
            for (const line of lines) {
                if (line.includes('SerialNumber')) {
                    const serial = line.split('=')[1].trim();
                    return crypto.createHash('sha256').update(serial).digest('hex');
                }
            }
        }
        
        // Linux平台：获取块设备UUID
        if (process.platform === 'linux') {
            // 获取当前挂载的USB设备UUID
            const cmd = 'blkid -o value -s UUID $(mount | grep "/dev/sd" | head -1 | awk \'{print $1}\')';
            const uuid = execSync(cmd, { encoding: 'utf8' }).trim();
            
            // 获取分区序列号
            const serialCmd = 'lsblk -o UUID,SERIAL -n $(mount | grep "/dev/sd" | head -1 | awk \'{print $1}\')';
            const serialOutput = execSync(serialCmd, { encoding: 'utf8' }).trim();
            
            return crypto.createHash('sha256').update(uuid + serialOutput).digest('hex');
        }
        
        // macOS平台：获取磁盘标识符
        if (process.platform === 'darwin') {
            const cmd = 'diskutil info $(mount | grep "/Volumes/" | head -1 | awk \'{print $1}\')';
            const output = execSync(cmd, { encoding: 'utf8' });
            
            // 提取UUID和序列号
            const uuidMatch = output.match(/Volume UUID:\s+([0-9A-F-]+)/);
            const serialMatch = output.match(/Serial Number:\s+([0-9A-Z]+)/);
            
            const uuid = uuidMatch ? uuidMatch[1] : '';
            const serial = serialMatch ? serialMatch[1] : '';
            
            return crypto.createHash('sha256').update(uuid + serial).digest('hex');
        }
    } catch (error) {
        console.error('获取硬件ID失败:', error.message);
        return crypto.createHash('sha256').update('fallback_id').digest('hex');
    }
    
    return crypto.createHash('sha256').update('default_id').digest('hex');
}

/**
 * 验证硬件绑定
 * @returns {boolean} 验证是否成功
 */
function verifyHardware() {
    const currentID = getUSBHardwareID();
    
    // 读取存储的硬件ID
    let storedID = '';
    try {
        storedID = fs.readFileSync('./config/hardware.bin', 'utf8').trim();
    } catch (error) {
        // 首次运行，初始化硬件绑定
        console.log('首次运行，初始化硬件绑定...');
        fs.writeFileSync('./config/hardware.bin', currentID);
        storedID = currentID;
    }
    
    // 验证匹配
    if (currentID !== storedID) {
        console.error('授权验证失败：硬件ID不匹配');
        console.log(`当前硬件ID: ${currentID}`);
        console.log(`存储硬件ID: ${storedID}`);
        
        // 尝试软匹配（允许小部分差异）
        const similarity = calculateSimilarity(currentID, storedID);
        if (similarity < 0.95) {
            console.error('硬件ID差异过大，拒绝启动');
            return false;
        } else {
            console.warn('硬件ID轻微变化，可能是设备更新，继续启动...');
        }
    }
    
    console.log('硬件验证成功');
    return true;
}

/**
 * 计算字符串相似度
 * @param {string} str1 字符串1
 * @param {string} str2 字符串2
 * @returns {number} 相似度（0-1）
 */
function calculateSimilarity(str1, str2) {
    const len = Math.max(str1.length, str2.length);
    let matches = 0;
    
    for (let i = 0; i < len; i++) {
        if (str1[i] === str2[i]) matches++;
    }
    
    return matches / len;
}

/**
 * 初始化硬件绑定（首次安装时调用）
 */
function initializeHardwareBinding() {
    const hardwareID = getUSBHardwareID();
    
    // 创建配置目录
    if (!fs.existsSync('./config')) {
        fs.mkdirSync('./config', { recursive: true });
    }
    
    // 存储硬件ID
    fs.writeFileSync('./config/hardware.bin', hardwareID);
    
    // 生成初始授权文件
    const licenseData = {
        licenseKey: crypto.randomBytes(32).toString('hex'),
        usb_id: hardwareID,
        created_at: new Date().toISOString(),
        version: '1.0',
        features: ['basic'],
        expiration_date: null, // 永久授权
        status: 'active'
    };
    
    fs.writeFileSync('./config/license.json', JSON.stringify(licenseData, null, 2));
    
    console.log('硬件绑定初始化完成');
    console.log(`硬件ID: ${hardwareID}`);
    console.log(`授权密钥: ${licenseData.licenseKey}`);
}

module.exports = {
    getUSBHardwareID,
    verifyHardware,
    initializeHardwareBinding,
    calculateSimilarity
};

// 导出用于测试
if (require.main === module) {
    const hardwareID = getUSBHardwareID();
    console.log('当前硬件ID:', hardwareID);
    
    // 尝试验证
    verifyHardware();
}