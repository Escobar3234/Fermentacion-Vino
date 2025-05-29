#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ==== CONFIGURACIÓN WiFi ====
const char* ssid = "#YoSoyFet - ESTUDIANTES";
const char* password = "YoSoyFet2025*";

// ==== OBJETOS DE SENSOR ====
#define ONE_WIRE_BUS 15
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
Adafruit_BMP280 bmp;

// ==== LECTURA ACTUAL ====
struct Lectura {
  int hora;
  float temperatura;
  float presion;
};
Lectura lecturaActual;
int horaActual = 1;

#define MAX_LECTURAS 24
Lectura historial1m[MAX_LECTURAS];
int totalLecturas1m = 0;

// ==== WEBSERVER ====
WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi conectado. IP: " + WiFi.localIP().toString());

  ds18b20.begin();
  if (!bmp.begin(0x76)) {
    Serial.println("No se encontró el BMP280!"); while (1);
  }

  server.begin();
}

void loop() {
  static unsigned long ultimaLectura = 0;
  if (millis() - ultimaLectura >= 10000) {
    ds18b20.requestTemperatures();
    float temp = ds18b20.getTempCByIndex(0);
    float presion = bmp.readPressure() / 100.0F;
    lecturaActual = {horaActual++, temp, presion};
    ultimaLectura = millis();

    if (totalLecturas1m < MAX_LECTURAS) {
      historial1m[totalLecturas1m++] = lecturaActual;
    } else {
      for (int i = 1; i < MAX_LECTURAS; i++) {
        historial1m[i - 1] = historial1m[i];
      }
      historial1m[MAX_LECTURAS - 1] = lecturaActual;
    }
  }

  WiFiClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        String req = client.readStringUntil('\r');
        client.read();  // consume el '\n'

        if (req.indexOf("GET /datos") >= 0) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: application/json");
          client.println("Access-Control-Allow-Origin: *");
          client.println();
          client.printf("{\"hora\":%d,\"temperatura\":%.2f,\"presion\":%.2f}",
            lecturaActual.hora, lecturaActual.temperatura, lecturaActual.presion);
        } 
        else if (req.indexOf("GET /tabla1m") >= 0) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Access-Control-Allow-Origin: *");
          client.println();
          client.println(generarTabla1mHTML());
        }
        else {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();
          client.println(generarHTML());
        }
        break;
      }
    }
    client.stop();
  }
}

String generarTabla1mHTML() {
  String tabla = "<table border='1'><tr><th>Hora</th><th>Temp (°C)</th><th>Presión (hPa)</th></tr>";
  for (int i = 0; i < totalLecturas1m; i++) {
    tabla += "<tr><td>" + String(historial1m[i].hora) + "</td><td>" +
             String(historial1m[i].temperatura, 2) + "</td><td>" +
             String(historial1m[i].presion, 2) + "</td></tr>";
  }
  tabla += "</table>";
  return tabla;
}

String generarHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Lecturas ESP32</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { font-family: Arial; padding: 10px; background: #f8f5f0; }
    h2 { color: maroon; }
    canvas { max-width: 100%; height: 250px !important; }
    table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    th, td { border: 1px solid #ccc; padding: 5px; text-align: center; }
    th { background-color: #f2dede; }
  </style>
</head>
<body>
  <h2>Lectura Actual</h2>
  <p><b>Hora:</b> <span id="hora"></span></p>
  <p><b>Temperatura:</b> <span id="temp"></span> °C</p>
  <p><b>Presión:</b> <span id="pres"></span> hPa</p>

  <h2>Gráfico de Temperatura y Presión</h2>
  <canvas id="grafico"></canvas>

  <h2>Tabla por hora</h2>
  <div id="tabla1m">Cargando...</div>

  <script>
    const ctx = document.getElementById('grafico').getContext('2d');
    const datos = {
      labels: [],
      datasets: [
        {
          label: 'Temperatura (°C)',
          borderColor: 'red',
          data: [],
          fill: false
        },
        {
          label: 'Presión (hPa)',
          borderColor: 'blue',
          data: [],
          fill: false
        }
      ]
    };

    const opciones = {
      scales: {
        y: {
          beginAtZero: false
        }
      }
    };

    const miGrafico = new Chart(ctx, {
      type: 'line',
      data: datos,
      options: opciones
    });

    async function actualizarDatos() {
      const res = await fetch('/datos');
      const data = await res.json();

      // actualizar texto
      document.getElementById('hora').textContent = data.hora;
      document.getElementById('temp').textContent = data.temperatura.toFixed(2);
      document.getElementById('pres').textContent = data.presion.toFixed(2);

      // actualizar gráfico
      datos.labels.push('H' + data.hora);
      datos.datasets[0].data.push(data.temperatura);
      datos.datasets[1].data.push(data.presion);
      if (datos.labels.length > 10) {
        datos.labels.shift();
        datos.datasets[0].data.shift();
        datos.datasets[1].data.shift();
      }
      miGrafico.update();
    }

    async function actualizarTabla1m() {
      const res = await fetch('/tabla1m');
      const html = await res.text();
      document.getElementById('tabla1m').innerHTML = html;
    }

    setInterval(actualizarDatos, 10000);
    setInterval(actualizarTabla1m, 60000);

    actualizarDatos();
    actualizarTabla1m();
  </script>
</body>
</html>
)rawliteral";
}
