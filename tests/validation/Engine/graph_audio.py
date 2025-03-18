import numpy as np
import matplotlib.pyplot as plt
import os
import argparse

# Global variable for line width
LINEWIDTH = 0.5

def read_pcm(file_path, sample_rate=44100, dtype=np.int16, num_channels=1):
    with open(file_path, 'rb') as f:
        pcm_data = np.frombuffer(f.read(), dtype=dtype)
    # Reshape the data to separate channels if there are multiple channels
    if num_channels > 1:
        pcm_data = pcm_data.reshape(-1, num_channels)
    return pcm_data

def plot_waveforms(file_path1, file_path2, pcm_data1, pcm_data2, sample_rate, output_file, downsample_factor=10, time_window=None, num_channels1=1, num_channels2=1):
    # Downsample the data
    pcm_data1 = pcm_data1[::downsample_factor]
    pcm_data2 = pcm_data2[::downsample_factor]
    sample_rate = sample_rate // downsample_factor

    # Ensure both data arrays have the same length
    min_length = min(len(pcm_data1), len(pcm_data2))
    pcm_data1 = pcm_data1[:min_length]
    pcm_data2 = pcm_data2[:min_length]

    # If a time window is specified, plot only that subset
    if time_window:
        start_sample = int(time_window[0] * sample_rate)
        end_sample = int(time_window[1] * sample_rate)
        pcm_data1 = pcm_data1[start_sample:end_sample]
        pcm_data2 = pcm_data2[start_sample:end_sample]

    time_axis = np.arange(len(pcm_data1)) / sample_rate

    # Extract file names from paths
    file_name1 = os.path.basename(file_path1)
    file_name2 = os.path.basename(file_path2)

    # Create a figure with a grid of subplots
    num_plots = 1 + num_channels1 + num_channels2
    fig, axs = plt.subplots(num_plots, 1, figsize=(10, 2 * num_plots), gridspec_kw={'height_ratios': [2] + [1] * (num_plots - 1)})

    # Main plot for combined waveforms
    if num_channels1 > 1:
        axs[0].plot(time_axis, pcm_data1[:, 0], color='red', linewidth=LINEWIDTH, label=f'{file_name1} - Left')
        axs[0].plot(time_axis, pcm_data1[:, 1], color='black', linewidth=LINEWIDTH, label=f'{file_name1} - Right')
    else:
        axs[0].plot(time_axis, pcm_data1, color='red', linewidth=LINEWIDTH, label=file_name1)

    if num_channels2 > 1:
        axs[0].plot(time_axis, pcm_data2[:, 0], color='blue', linewidth=LINEWIDTH, label=f'{file_name2} - Left')
        axs[0].plot(time_axis, pcm_data2[:, 1], color='green', linewidth=LINEWIDTH, label=f'{file_name2} - Right')
    else:
        axs[0].plot(time_axis, pcm_data2, color='blue', linewidth=LINEWIDTH, label=file_name2)

    axs[0].set_title('Combined Waveforms')
    axs[0].set_xlabel('Time [s]')
    axs[0].set_ylabel('Amplitude')
    axs[0].grid(True)
    axs[0].legend()

    # Plot each channel separately
    plot_index = 1
    if num_channels1 > 1:
        axs[plot_index].plot(time_axis, pcm_data1[:, 0], color='red', linewidth=LINEWIDTH)
        axs[plot_index].set_title(f'{file_name1} - Left Channel')
        axs[plot_index].set_xlabel('Time [s]')
        axs[plot_index].set_ylabel('Amplitude')
        axs[plot_index].grid(True)
        plot_index += 1

        axs[plot_index].plot(time_axis, pcm_data1[:, 1], color='black', linewidth=LINEWIDTH)
        axs[plot_index].set_title(f'{file_name1} - Right Channel')
        axs[plot_index].set_xlabel('Time [s]')
        axs[plot_index].set_ylabel('Amplitude')
        axs[plot_index].grid(True)
        plot_index += 1
    else:
        axs[plot_index].plot(time_axis, pcm_data1, color='red', linewidth=LINEWIDTH)
        axs[plot_index].set_title(file_name1)
        axs[plot_index].set_xlabel('Time [s]')
        axs[plot_index].set_ylabel('Amplitude')
        axs[plot_index].grid(True)
        plot_index += 1

    if num_channels2 > 1:
        axs[plot_index].plot(time_axis, pcm_data2[:, 0], color='blue', linewidth=LINEWIDTH)
        axs[plot_index].set_title(f'{file_name2} - Left Channel')
        axs[plot_index].set_xlabel('Time [s]')
        axs[plot_index].set_ylabel('Amplitude')
        axs[plot_index].grid(True)
        plot_index += 1

        axs[plot_index].plot(time_axis, pcm_data2[:, 1], color='green', linewidth=LINEWIDTH)
        axs[plot_index].set_title(f'{file_name2} - Right Channel')
        axs[plot_index].set_xlabel('Time [s]')
        axs[plot_index].set_ylabel('Amplitude')
        axs[plot_index].grid(True)
    else:
        axs[plot_index].plot(time_axis, pcm_data2, color='blue', linewidth=LINEWIDTH)
        axs[plot_index].set_title(file_name2)
        axs[plot_index].set_xlabel('Time [s]')
        axs[plot_index].set_ylabel('Amplitude')
        axs[plot_index].grid(True)

    # Adjust layout to prevent overlap
    plt.tight_layout()
    plt.savefig(output_file)
    plt.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot waveforms from PCM files.")
    parser.add_argument('file_path1', type=str, help='Path to the first PCM file.')
    parser.add_argument('file_path2', type=str, help='Path to the second PCM file.')
    parser.add_argument('--sample_rate', type=int, default=48000, help='Sample rate of the PCM files.')
    parser.add_argument('--output_file', type=str, default='waveform.png', help='Output file name for the waveform plot.')
    parser.add_argument('--num_channels1', type=int, default=2, help='Number of channels in the first PCM file.')
    parser.add_argument('--num_channels2', type=int, default=2, help='Number of channels in the second PCM file.')
    parser.add_argument('--downsample_factor', type=int, default=10, help='Factor by which to downsample the data.')
    parser.add_argument('--time_window', type=float, nargs=2, default=(0, 0.03), help='Time window to plot (start, end) in seconds.')

    args = parser.parse_args()

    pcm_data1 = read_pcm(args.file_path1, args.sample_rate, num_channels=args.num_channels1)
    pcm_data2 = read_pcm(args.file_path2, args.sample_rate, num_channels=args.num_channels2)
    plot_waveforms(args.file_path1, args.file_path2, pcm_data1, pcm_data2, args.sample_rate, args.output_file, downsample_factor=args.downsample_factor, time_window=args.time_window, num_channels1=args.num_channels1, num_channels2=args.num_channels2)

    print(f"Waveform saved to {args.output_file}")