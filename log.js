const mqtt = require('mqtt');

console.log("📡 Starting Remote Serial Monitor...\n");

// ඔයාගේ HiveMQ Credentials
const mqttUrl = "mqtts://65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud:8883";
const mqttOptions = {
    username: "esp32",
    password: "Thilinakavishan32@gmail.com",
    clientId: `serial_monitor_${Math.random().toString(16).slice(3)}`
};

const client = mqtt.connect(mqttUrl, mqttOptions);

client.on('connect', () => {
    console.log("✅ Connected to HiveMQ Server!");
    console.log("⏳ Waiting for ESP32 logs...\n");
    console.log("==================================================");
    
    // Logs සහ Status එන Topics දෙකටම Subscribe වෙනවා
    client.subscribe('board/logs', { qos: 0 });
    client.subscribe('board/status', { qos: 0 });
});

client.on('message', (topic, message) => {
    // මැසේජ් එක ආපු වෙලාව ගන්නවා
    const time = new Date().toLocaleTimeString();
    const msgString = message.toString();

    // Topic එක අනුව ලස්සනට Terminal එකේ Print කරනවා
    if (topic === 'board/logs') {
        console.log(`[${time}] 📝 LOG: ${msgString}`);
    } 
    else if (topic === 'board/status') {
        if (msgString === 'Online') {
            console.log(`[${time}] 🟢 STATUS: ESP32 is ONLINE`);
        } else {
            console.log(`[${time}] 🔴 STATUS: ESP32 went OFFLINE`);
        }
    }
});

client.on('error', (err) => {
    console.error("❌ MQTT Connection Error:", err);
});

// `Ctrl + C` ඔබලා මේක නවත්තද්දි ලස්සනට Disconnect වෙන්න
process.on('SIGINT', () => {
    console.log("\n🛑 Stopping Remote Serial Monitor...");
    client.end();
    process.exit();
});