/*
 * Simple heads up display 
 *
 * To run this sketch you will have to install processing-mqtt:
 * https://github.com/256dpi/processing-mqtt
 */
import processing.mqtt.*;

MQTTClient client;

String lastCaliperMessage = "No message";
String calipherTopic = "/18:fe:34:9c:fc:d2";
String lastDialMessage = "0.0"; 
float dialOffset = 0.0;
String dialTopic = "/18:fe:34:9c:fc:d2"; //"/18:fe:34:9c:4c:bc";

void setup() {
  client = new MQTTClient(this);;
  size(displayWidth, displayHeight);
  textFont(createFont("Georgia", 90));
  client.connect("mqtt://demo:demo@192.168.0.10:8080", "my-client");
  client.subscribe(calipherTopic);
  client.subscribe(dialTopic);
}

void draw() {
  background( 0 );
  fill( 255 );
  textAlign(CENTER, CENTER);
  float xBorder = 0.1;
  float yBorder = 0.3;
  float dialSample = Float.parseFloat(lastDialMessage) - dialOffset;
  text("Dial : " + String.format("%.4f", dialSample) + " mm", displayWidth*xBorder, displayHeight*yBorder - displayHeight*0.2, displayWidth*(1.0-xBorder*2.), displayHeight*(1.0-yBorder*2.));
  //text("Caliper: " + lastCaliperMessage + " mm", displayWidth*xBorder, displayHeight*yBorder - displayHeight*0.2, displayWidth*(1.0-xBorder*2.), displayHeight*(1.0-yBorder*2.));
}

void keyPressed() {
  dialOffset = Float.parseFloat(lastDialMessage);
}

void messageReceived(String topic, byte[] payload) {
  if (topic.equals(calipherTopic)) {
    lastCaliperMessage = new String(payload);
    println("new message: " + topic + " - " + lastCaliperMessage);
  } else if (topic.equals(dialTopic)) {
    lastDialMessage = new String(payload);
    println("new message: " + topic + " - " + lastDialMessage);
  } else {
     println("Unknown topic: >" + topic + "< - " + new String(payload));
  }
}
