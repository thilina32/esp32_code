const mqtt = require('mqtt');

// ඔයාගේ HiveMQ Broker විස්තර
const options = {
  host: '65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud',
  port: 8883,
  protocol: 'mqtts', // Secure connection එකක් නිසා mqtts පාවිච්චි කළ යුතුයි
  username: 'esp32',
  password: 'Thilinakavishan32@gmail.com'
};

console.log("🔄 MQTT Broker එකට සම්බන්ධ වෙමින් පවතී...");

// Broker එකට connect වීම
const client = mqtt.connect(options);

client.on('connect', () => {
  console.log("✅ සම්බන්ධ විය! Reset Signal එක යවමින්...");

  // "board/control" topic එකට "ALARM_OFF" message එක යැවීම
  client.publish('board/control', 'ALARM_OFF', (err) => {
    if (!err) {
      console.log("🛡️ Alarm එක සාර්ථකව Reset කරන ලදී!");
    } else {
      console.error("❌ Reset Signal එක යැවීමේදී දෝෂයක්:", err);
    }
    
    // මැසේජ් එක යවලා ඉවර වුණාම connection එක close කරනවා
    client.end();
  });
});

client.on('error', (err) => {
  console.error("❌ සම්බන්ධ වීමේ දෝෂයක්:", err);
  client.end();
});