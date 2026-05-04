

import processing.serial.*;
import java.awt.Color;

int sensorLineReading = 0;
float ambientTemperature = 0.0;
float sensorValues[][] = new float[24][32];
ArrayList<float []> boundingBoxesRecv = new ArrayList<float []>();
float boundingBoxesToShow[][] = new float[0][5];

void setup() {
  size(32*20, 24*20);
  String portList[] = Serial.list();
  println("Available serial ports: ");
  for (int i = 0; i < portList.length; i++) {
    println(i + ": " + portList[i]);
  }
  
  int portIndex = 0; // Change this to the index of your serial port

  //check if the first port starts with /dev/
  if (portList[0].startsWith("/dev/")) {
    //this seems to be a mac
    for (int i = 0; i < portList.length; i++) {
      if (portList[i].contains("cu.usbserial")) {
        portIndex = i;
        break;
      }
    }
  }
  
  Serial myPort = new Serial(this, portList[portIndex], 115200); // Adjust the port index as needed
  myPort.bufferUntil('\n');
  myPort.clear();
}

void draw() {
  background(100);
  noStroke();
  for (int i = 0; i < 24; i++) {
    for (int j = 0; j < 32; j++) {
      float value = sensorValues[i][j];

      float min_temp = 25*1;
      float max_temp = 35*1;

      float mappedRatio = map(value, min_temp, max_temp, 0.6666, 0); // Map the sensor value to a color range
      mappedRatio = constrain(mappedRatio, 0, 0.6666); // Ensure the value is within the range
      //convert the mapped ratio to a color, using jet colormap, convert it through HSB
      int myColor = Color.HSBtoRGB(mappedRatio, 1, 1);
      fill(myColor); // Color based on temperature
      rect(j * 20, i * 20, 20, 20);
    }
  }

  for (int i = 0; i < boundingBoxesToShow.length; i++) {
    float[] box = boundingBoxesToShow[i];
    float x = box[0] * 20;
    float y = box[1] * 20;
    float w = box[2] * 20;
    float h = box[3] * 20;
    float confidence = box[4];
    stroke(255, 255, 255, confidence * 255); // Set stroke color based on confidence
    strokeWeight(2);
    noFill();
    rect(x, y, w, h);
  }

  noStroke();
  textSize(30);
  fill(255, 255, 255);
  text("Ambient Temperature: " + ambientTemperature + " °C", 10, 30);
}

void serialEvent(Serial myPort) {
  String data = myPort.readStringUntil('\n');
  if (data != null) {
    data = data.trim(); // Remove any leading or trailing whitespace
    if (data.length() > 0) {
      if (data.startsWith("============")) {
        sensorLineReading = 0; // Reset the sensorLineReading when a new line starts
        boundingBoxesToShow = new float[boundingBoxesRecv.size()][5]; // Initialize the bounding boxes array
        for (int i = 0; i < boundingBoxesRecv.size(); i++) {
          boundingBoxesToShow[i] = boundingBoxesRecv.get(i); // Copy the bounding boxes to show
        }
        boundingBoxesRecv.clear(); // Clear the received bounding boxes for the next iteration
      } else if (data.startsWith("Ambient temperature")) {
        // Extract the temperature value from the string between the "=" and degC
        String[] parts = data.split("=");
        if (parts.length > 1) {
          String tempString = parts[1].trim(); // Get the part after "="
          String[] tempParts = tempString.split("degC");
          if (tempParts.length > 0) {
            String temperature = tempParts[0].trim(); // Get the temperature value
            ambientTemperature = Float.parseFloat(temperature); // Convert to float
          }
        }
      } else if (data.startsWith("Detection")) {
        String[] parts = data.split(",");
        if (parts.length == 6) {
          float[] boundingBox = new float[5];
          for (int i = 0; i < 5; i++) {
            boundingBox[i] = Float.parseFloat(parts[i+1].trim());
          }
          boundingBoxesRecv.add(boundingBox); // Add the bounding box to the list
        }
      } else {
        String[] sensorData = data.split(",");
        if (sensorData.length == 32) {
          for (int i = 0; i < sensorData.length; i++) {
            float sensorValue = Float.parseFloat(sensorData[i].trim());
            if (sensorLineReading < 24) {
              sensorValues[sensorLineReading][i] = sensorValue; // Store the sensor value in the array
            } 
          }
          sensorLineReading += 1; // Increment the line reading index
        }
      }
    }
  }
}
