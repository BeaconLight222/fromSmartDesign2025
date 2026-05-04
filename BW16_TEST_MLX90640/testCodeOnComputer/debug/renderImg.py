import os
import matplotlib.pyplot as plt
import numpy as np

for i in range(28):
    print(i)
    filename = f"c_gen_float_{i:03d}.txt"
    if not os.path.exists(filename):
        print(f"File {filename} does not exist.")
        continue
    data = []
    with open(filename, 'r') as file:
        lines = file.readlines()
        for line in lines:
            numbers = line.split()
            for number in numbers:
                data.append(float(number))

    filename = f"c_gen_detection_{i:03d}.txt"
    if not os.path.exists(filename):
        print(f"File {filename} does not exist.")
        continue
    detections = []
    with open(filename, 'r') as file:
        lines = file.readlines()
        for line in lines:
            numbers = line.split()
            if len(numbers) == 5:
                x, y, w, h, conf = map(float, numbers)
                detections.append((x, y, w, h, conf))
    validDetections = len(detections)


    min_temp = 25*1
    max_temp = 35*1
    data = np.array(data)  # Convert to numpy array
    data = data.reshape((24, 32))  # Reshape to 24x32
    thermal_data = np.clip(data, min_temp, max_temp)  # Clip the values to the range
    thermal_data = (thermal_data - min_temp) * 255.0/ (max_temp - min_temp)   # Normalize to 0-255
    thermal_data = thermal_data.astype(np.uint8)  # Convert to uint8

    colormap = plt.get_cmap('jet', 256)  # Use 'jet' colormap with 256 colors
    color_image = colormap(thermal_data)[:, :, :3]  # Get RGB values (ignore alpha channel)
    color_image = (color_image * 255).astype(np.uint8)  # Convert to uint8
    scaleFactor = 32
    #scale up to 16x
    color_image = np.repeat(np.repeat(color_image, scaleFactor, axis=0), scaleFactor, axis=1)

    for detection in detections:
        print("detection:", detection)
        x = int(detection[0] * scaleFactor)
        y = int(detection[1] * scaleFactor)
        w = int(detection[2] * scaleFactor)
        h = int(detection[3] * scaleFactor)
        conf = detection[4]
        #draw the box on the image, on sides only

        x = np.clip(x, 0, color_image.shape[1]-1)
        y = np.clip(y, 0, color_image.shape[0]-1)
        w = np.clip(w, 0, color_image.shape[1]-x-1)
        h = np.clip(h, 0, color_image.shape[0]-y-1)

        conf = np.clip(conf, 0, 1)
        #map the confidence to a color, 0.3 is red, 0.8 or more is green
        confMap = np.clip((conf - 0.3) / (0.8 - 0.3), 0, 1)
        confMap = confMap * 0.5 + 0.5
        color = (1-confMap) * np.array([1, 0, 0]) + confMap * np.array([0, 1, 0])
        color = (color * 255).astype(np.uint8)
        # print("color:", color)
        color_image[y:y+h, x:x+1, :] = color
        color_image[y:y+h, x+w-1:x+w, :] = color
        color_image[y:y+1, x:x+w, :] = color
        color_image[y+h-1:y+h, x:x+w, :] = color
    plt.imsave(f"output_{i:03d}.jpg", color_image)

    