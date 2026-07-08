const mqtt = require('mqtt');

// ඔයාගේ HiveMQ Broker විස්තර
const options = {
  host: '65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud',
  port: 8883,
  protocol: 'mqtts', 
  username: 'esp32',
  password: 'Thilinakavishan32@gmail.com'
};

console.log("🔄 MQTT Broker එකට සම්බන්ධ වෙමින් පවතී...");

const client = mqtt.connect(options);

client.on('connect', () => {
  console.log("✅ සම්බන්ධ විය! Radar SCAN Command එක යවමින්...");

  // යවන්න ඕන මැසේජ් එක "SCAN" ලෙස සකසා ඇත
  const message = 'SCAN';

  // "board/control" topic එකට මැසේජ් එක යැවීම
  client.publish('board/control', message, (err) => {
    if (!err) {
      console.log(`📡 සාර්ථකව යැව්වා Command: ${message}`);
      console.log("🔍 ESP32 එක දැන් Scan කිරීම ආරම්භ කරනු ඇත...");
    } else {
      console.error("❌ Command එක යැවීමේදී දෝෂයක්:", err);
    }
    
    // මැසේජ් එක යවලා ඉවර වුණාම connection එක close කරනවා
    client.end();
  });
});

client.on('error', (err) => {
  console.error("❌ සම්බන්ධ වීමේ දෝෂයක්:", err);
  client.end();
});