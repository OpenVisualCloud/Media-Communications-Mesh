import sys
import pytesseract
import cv2 as cv
import numpy as np
from datetime import datetime
import re
import matplotlib.pyplot as plt
from concurrent.futures import ThreadPoolExecutor
import os

def is_display_attached():
    # Check if the DISPLAY environment variable is set
    return 'DISPLAY' in os.environ

def extract_text_from_region(image, x, y, font_size, length):
    """
    Extracts text from a specific region of the image.
    :param image: The image to extract text from.
    :param x: The x-coordinate of the top-left corner of the region.
    :param y: The y-coordinate of the top-left corner of the region.
    :param font_size: The font size of the text.
    :param length: The length of the text to extract.
    :return: The extracted text.
    """
    margin = 5
    y_adjusted = max(0, y - margin)
    x_adjusted = max(0, x - margin)
    height = y + font_size + margin
    width = x + length + margin
    # Define the region of interest (ROI) for text extraction
    roi = image[y_adjusted:height, x_adjusted:width]

    # Use Tesseract to extract text from the ROI
    return pytesseract.image_to_string(roi, lang='eng')

def process_frame(frame_idx, frame):
    print("Processing Frame: ", frame_idx)

    timestamp_format = "%H:%M:%S:%f"
    timestamp_pattern = r'\b\d{2}:\d{2}:\d{2}:\d{3}\b'

    # Convert frame to grayscale for better OCR performance
    frame = cv.cvtColor(frame, cv.COLOR_BGR2GRAY)

    line_1 = extract_text_from_region(frame, 10, 10, 50, 700)
    line_2 = extract_text_from_region(frame, 10, 70, 50, 700)

    # Find the timestamps(Type: string) in the extracted text using regex
    tx_time = re.search(timestamp_pattern, line_1)
    rx_time = re.search(timestamp_pattern, line_2)

    if tx_time is None or rx_time is None:
        print("Error: Timestamp not found in the expected format.")
        return 0

    # Convert the timestamps(Type: string) to time (Type: datetime)
    tx_time = datetime.strptime(tx_time.group(), timestamp_format)
    rx_time = datetime.strptime(rx_time.group(), timestamp_format)

    if tx_time is None or rx_time is None:
        print("Error: Timestamp not found in the expected format.")
        return 0

    if tx_time > rx_time:
        print("Error: Transmit time is greater than receive time.")
        return 0

    time_difference = rx_time - tx_time
    time_difference_ms = time_difference.total_seconds() * 1000
    return time_difference_ms

def main():
    if len(sys.argv) < 2:
        print("Usage: python text-detection.py <input_video_file> <output_image_name>")
        sys.exit(1)

    cap = cv.VideoCapture(sys.argv[1])
    if not cap.isOpened():
        print("Error: Could not open video file.")
        sys.exit(1)
   
    frame_idx = 0
    time_differences = []

    with ThreadPoolExecutor(max_workers=40) as executor:
        futures = []
        while True:
            ret, frame = cap.read()
            if not ret:
                break

            futures.append(executor.submit(process_frame, frame_idx, frame))
            frame_idx += 1

        for future in futures:
            time_differences.append(future.result())

    cap.release()

    plt.plot(time_differences, marker='o')
    plt.title('Media Communications Mesh Latency')
    plt.xlabel('Frame Index')
    plt.ylabel('Measured latency (ms)')
    plt.grid(True)
    if is_display_attached():
        plt.show()

    if len(sys.argv) == 3:
        filename = sys.argv[2]
        if not filename.endswith('.jpg'):
            filename += '.jpg'
        print("Saving the plot to: ", filename)
        plt.savefig(filename, format='jpg', dpi=300)
    
main()
