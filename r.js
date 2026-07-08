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

// 👇 මෙතනින් ඔයාට ඕන Servo එකයි, හැරෙන්න ඕන අංශක ගානයි වෙනස් කරන්න
const targetServo = 'SERVO1'; // SERVO1 හෝ SERVO2 දෙන්න
const angle = 180;             // 0 ත් 180 ත් අතර අගයක් දෙන්න

client.on('connect', () => {
  console.log("✅ සම්බන්ධ විය! Servo Control Signal එක යවමින්...");

  // යවන්න ඕන මැසේජ් එක හදාගන්නවා (උදා: "SERVO1:45")
  const message = `${targetServo}:${angle}`;

  // "board/control" topic එකට මැසේජ් එක යැවීම
  client.publish('board/control', message, (err) => {
    if (!err) {
      console.log(`🔧 සාර්ථකව යැව්වා: ${message}`);
    } else {
      console.error("❌ Signal එක යැවීමේදී දෝෂයක්:", err);
    }
    
    // මැසේජ් එක යවලා ඉවර වුණාම connection එක close කරනවා
    client.end();
  });
});

client.on('error', (err) => {
  console.error("❌ සම්බන්ධ වීමේ දෝෂයක්:", err);
  client.end();
});