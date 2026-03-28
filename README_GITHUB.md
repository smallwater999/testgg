# OpenClaw USB Business Package

## Overview
This repository contains a complete technical solution for packaging OpenClaw AI assistant into USB flash drives with encryption and licensing protection for commercial sales.

## Project Structure

### 1. Technical Documentation
- **OpenClaw_U盘商业封装方案.md** - Complete technical implementation guide
  - System architecture design
  - Three-layer security framework (hardware binding, runtime encryption, license verification)
  - Anti-crack strategies
  - Implementation roadmap

### 2. Code Samples
- **hardware-check.js** - Hardware binding verification module
  - Cross-platform USB device ID detection (Windows/Linux/Mac)
  - SHA256 hash calculation
  - Tolerance mechanism for device updates

- **license-system.js** - License management system
  - Offline and online license verification
  - Feature control
  - Heartbeat checking
  - Trial license creation

- **launch-scripts/**
  - **launch.sh** - Linux/Mac startup script
  - **Launch.cpp** - Windows startup program (C++)

- **config/default.yaml** - Configuration template

### 3. Business Plan
- **business-plan.md** - Commercial strategy and roadmap
  - Pricing tiers (Basic/Professional/Enterprise)
  - Market positioning
  - Risk assessment
  - Financial projections

## Key Features

### Anti-Crack Technology
1. **Hardware Binding** - USB device serial number/UUID based verification
2. **Multi-layer Encryption** - AES-256 encryption for core files
3. **Runtime Protection** - File integrity checking and anti-debug protection
4. **License System** - Online/offline verification with heartbeat checks

### Commercial Features
1. **Three-tier Pricing**
   - Basic: $99 one-time purchase
   - Professional: $299/year subscription
   - Enterprise: $999+/year custom licensing

2. **Sales Channels**
   - Official website
   - E-commerce platforms (Amazon, Taobao, JD.com)
   - Technology exhibitions
   - University partnerships

## Quick Start

### Step 1: Hardware Binding Implementation
```javascript
// Test hardware binding
const hardware = require('./hardware-check.js');
const hardwareID = hardware.getUSBHardwareID();
console.log('Hardware ID:', hardwareID);
```

### Step 2: License System Setup
```javascript
// Create trial license
const license = require('./license-system.js');
license.createTrialLicense(7); // 7-day trial
```

### Step 3: Build USB Package
```bash
# Create USB directory structure
mkdir USB_Package
mkdir USB_Package/runtime
mkdir USB_Package/config
mkdir USB_Package/skills

# Copy OpenClaw distribution
cp openclaw-dist/ USB_Package/runtime/

# Copy Node.js portable
cp node-v22.22.1/ USB_Package/runtime/

# Add startup scripts
cp launch.sh USB_Package/
cp Launch.cpp USB_Package/
```

## Legal Compliance
- Based on MIT licensed OpenClaw project
- Original copyright notices must be preserved
- Cannot claim to be original developer
- Clear attribution to OpenClaw

## License
This project includes MIT license from OpenClaw and proprietary commercial license for the USB packaging solution.

## Contact
For commercial licensing inquiries or technical support, contact:
Email: licensing@openclaw-portable.com
Website: https://openclaw-portable.com