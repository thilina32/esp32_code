const { execSync } = require('child_process');
const mqtt = require('mqtt');
const path = require('path');
const fs = require('fs');

console.log("🚀 Starting Cloud Auto-Update Pipeline...\n");

const projectPath = __dirname;
// ෆෝල්ඩර් එකේ නම ඔටෝම අරගන්නවා (උදා: esp32_code)
const folderName = path.basename(projectPath); 

// ෆෝල්ඩර් එකේ නමට සමාන .ino ෆයිල් එක හොයනවා
const inoFile = `${folderName}.ino`; 
const compiledBin = path.join(projectPath, `${folderName}.ino.bin`);

// ESP එක අලුත් Update එක විදියට හොයන්නේ code.bin නිසා මේක වෙනස් කරන්නේ නෑ
const finalBin = path.join(projectPath, 'code.bin');

// 0. Compilation කොටස (Arduino CLI) ----------------------------------
try {
    console.log(`🛠️ 1/4: Compiling ${inoFile} for ESP32-S3...`);
    const compileCommand = `arduino-cli compile --fqbn esp32:esp32:esp32s3 --output-dir . "${inoFile}"`;
    
    execSync(compileCommand, { stdio: 'inherit', cwd: projectPath });
    console.log("\n✅ Compilation Successful!");

} catch (error) {
    console.error("\n❌ Compilation Failed! Please check your code for errors.");
    process.exit(1); 
}

// 1. Rename .bin File -----------------------------------------------
try {
    if (fs.existsSync(compiledBin)) {
        // පරණ code.bin එකක් තියෙනවා නම් ඒක අයින් කරනවා
        if (fs.existsSync(finalBin)) {
            fs.unlinkSync(finalBin); 
        }
        // අලුත් එක code.bin කියලා Rename කරනවා
        fs.renameSync(compiledBin, finalBin);
        console.log("🔀 2/4: Binary file renamed to code.bin successfully.");
    } else {
        throw new Error(`Compiled binary file (${folderName}.ino.bin) not found!`);
    }
} catch (error) {
    console.error("\n❌ Failed to rename binary file:", error);
    process.exit(1);
}

// 2. Git Automation කොටස --------------------------------------------
try {
    console.log("\n📦 3/4: Adding files to Git...");
    // අලුත් ෆයිල්ස් ඔක්කොම එකපාර Git එකට ඇඩ් කරනවා
    execSync('git add .', { stdio: 'inherit', cwd: projectPath });

    const commitMsg = `Auto Cloud OTA Update: ${new Date().toLocaleString()}`;
    execSync(`git commit -m "${commitMsg}"`, { stdio: 'inherit', cwd: projectPath });

    console.log("☁️ Pushing to GitHub...");
    execSync('git push origin main', { stdio: 'inherit', cwd: projectPath });
    
    console.log("\n✅ Git Push Successful!");

} catch (error) {
    console.log("\n⚠️ Git process failed or no new changes to commit.");
}

// 3. MQTT Trigger කොටස ---------------------------------------------
console.log("\n⏳ Waiting 10 seconds for GitHub servers to refresh...");

setTimeout(() => {
    console.log("\n📡 4/4: Connecting to HiveMQ...");
    
    const mqttUrl = "mqtts://65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud:8883";
    const mqttClient = mqtt.connect(mqttUrl, {
        username: "esp32",
        password: "Thilinakavishan32@gmail.com",
        clientId: `cloud_worker_${Math.random().toString(16).slice(3)}`
    });

    mqttClient.on('connect', () => {
        console.log("✅ Connected to HiveMQ!");
        
        mqttClient.publish('board/update', 'START_OTA', { qos: 1 }, (err) => {
            if (err) {
                console.error("❌ Failed to send OTA trigger:", err);
            } else {
                console.log("🎯 'START_OTA' command sent to ESP32 successfully!");
            }
            
            mqttClient.end();
            console.log("\n🏁 Cloud Auto-Update Process 100% Complete!\n");
            process.exit(0);
        });
    });

    mqttClient.on('error', (err) => {
        console.error("❌ MQTT Connection Error:", err);
        process.exit(1);
    });

}, 10000);