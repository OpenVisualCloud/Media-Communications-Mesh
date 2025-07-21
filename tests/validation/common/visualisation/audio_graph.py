# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
import numpy as np
import matplotlib.pyplot as plt
import os
import argparse
from typing import Optional, Tuple, Union, List

# Global variable for line width
LINEWIDTH = 0.5

def read_pcm(file_path: str, sample_rate: int = 48000, pcm_format: int = 16, num_channels: int = 1) -> np.ndarray:
    """
    Read PCM audio data from a file.
    
    Args:
        file_path: Path to the PCM file.
        sample_rate: Sample rate of the PCM file (used for time calculations).
        pcm_format: PCM format (8, 16, or 24 bits).
        num_channels: Number of audio channels in the file.
        
    Returns:
        NumPy array containing the PCM data.
    """
    with open(file_path, 'rb') as f:
        if pcm_format == 8:
            pcm_data = np.frombuffer(f.read(), dtype=np.uint8) - 128  # Center around zero
        elif pcm_format == 16:
            pcm_data = np.frombuffer(f.read(), dtype=np.int16)
        elif pcm_format == 24:
            raw_data = np.frombuffer(f.read(), dtype=np.uint8)
            pcm_data = raw_data.reshape(-1, 3)
            pcm_data = np.left_shift(pcm_data[:, 0].astype(np.int32), 16) | \
                       np.left_shift(pcm_data[:, 1].astype(np.int32), 8) | \
                       pcm_data[:, 2].astype(np.int32)
            pcm_data = pcm_data - (pcm_data & 0x800000) * 2  # Sign extension
        else:
            raise ValueError(f"Unsupported PCM format '{pcm_format}'. Use 8, 16, or 24.")
    
    # Reshape the data to separate channels if there are multiple channels
    if num_channels > 1:
        pcm_data = pcm_data.reshape(-1, num_channels)
    return pcm_data

def downsample_pcm(pcm_data: np.ndarray, downsample_factor: int = 10) -> np.ndarray:
    """
    Downsample PCM data by a specified factor.
    
    Args:
        pcm_data: PCM data to downsample.
        downsample_factor: Factor by which to downsample the data.
        
    Returns:
        Downsampled PCM data.
    """
    return pcm_data[::downsample_factor]

def plot_waveforms(
    file_path1: str, 
    file_path2: str, 
    pcm_data1: np.ndarray, 
    pcm_data2: np.ndarray, 
    sample_rate: int, 
    output_file: str, 
    downsample_factor: int = 10, 
    start_time: Optional[float] = None, 
    end_time: Optional[float] = None, 
    num_channels1: int = 1, 
    num_channels2: int = 1
) -> None:
    """
    Plot waveforms from two PCM files for comparison.
    
    Args:
        file_path1: Path to the first PCM file (used for labeling).
        file_path2: Path to the second PCM file (used for labeling).
        pcm_data1: PCM data from the first file.
        pcm_data2: PCM data from the second file.
        sample_rate: Sample rate of the PCM files.
        output_file: Output file name for the waveform plot.
        downsample_factor: Factor by which to downsample the data.
        start_time: Start time in seconds for the plot.
        end_time: End time in seconds for the plot.
        num_channels1: Number of channels in the first PCM file.
        num_channels2: Number of channels in the second PCM file.
    """
    # Downsample the data
    pcm_data1 = downsample_pcm(pcm_data1, downsample_factor)
    pcm_data2 = downsample_pcm(pcm_data2, downsample_factor)
    effective_sample_rate = sample_rate // downsample_factor

    # Ensure both data arrays have the same length
    min_length = min(len(pcm_data1), len(pcm_data2))
    pcm_data1 = pcm_data1[:min_length]
    pcm_data2 = pcm_data2[:min_length]

    # If start_time or end_time is specified, plot only that subset
    if start_time is not None or end_time is not None:
        start_sample = int(start_time * effective_sample_rate) if start_time is not None else 0
        end_sample = int(end_time * effective_sample_rate) if end_time is not None else len(pcm_data1)
        pcm_data1 = pcm_data1[start_sample:end_sample]
        pcm_data2 = pcm_data2[start_sample:end_sample]

    time_axis = np.arange(len(pcm_data1)) / effective_sample_rate

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
    
    return os.path.abspath(output_file)

def plot_single_waveform(
    file_path: str,
    pcm_data: np.ndarray,
    sample_rate: int,
    output_file: str,
    downsample_factor: int = 10,
    start_time: Optional[float] = None,
    end_time: Optional[float] = None,
    num_channels: int = 1
) -> str:
    """
    Plot a waveform from a single PCM file.
    
    Args:
        file_path: Path to the PCM file (used for labeling).
        pcm_data: PCM data from the file.
        sample_rate: Sample rate of the PCM file.
        output_file: Output file name for the waveform plot.
        downsample_factor: Factor by which to downsample the data.
        start_time: Start time in seconds for the plot.
        end_time: End time in seconds for the plot.
        num_channels: Number of channels in the PCM file.
        
    Returns:
        Absolute path to the generated output file.
    """
    # Downsample the data
    pcm_data = downsample_pcm(pcm_data, downsample_factor)
    effective_sample_rate = sample_rate // downsample_factor

    # If start_time or end_time is specified, plot only that subset
    if start_time is not None or end_time is not None:
        start_sample = int(start_time * effective_sample_rate) if start_time is not None else 0
        end_sample = int(end_time * effective_sample_rate) if end_time is not None else len(pcm_data)
        pcm_data = pcm_data[start_sample:end_sample]

    time_axis = np.arange(len(pcm_data)) / effective_sample_rate
    file_name = os.path.basename(file_path)

    # Create a figure with a grid of subplots
    num_plots = 1 + (num_channels if num_channels > 1 else 0)
    fig, axs = plt.subplots(num_plots, 1, figsize=(10, 2 * num_plots), 
                            gridspec_kw={'height_ratios': [2] + [1] * (num_plots - 1)} if num_plots > 1 else None)
    
    # If only one plot, convert axs to a list for consistent indexing
    if num_plots == 1:
        axs = [axs]

    # Main plot for combined waveform
    if num_channels > 1:
        for i in range(num_channels):
            channel_name = 'Left' if i == 0 else 'Right' if i == 1 else f'Channel {i+1}'
            color = 'red' if i == 0 else 'blue' if i == 1 else f'C{i}'
            axs[0].plot(time_axis, pcm_data[:, i], color=color, linewidth=LINEWIDTH, 
                        label=f'{file_name} - {channel_name}')
    else:
        axs[0].plot(time_axis, pcm_data, color='red', linewidth=LINEWIDTH, label=file_name)

    axs[0].set_title('Waveform')
    axs[0].set_xlabel('Time [s]')
    axs[0].set_ylabel('Amplitude')
    axs[0].grid(True)
    axs[0].legend()

    # Plot each channel separately if multi-channel
    if num_channels > 1:
        for i in range(num_channels):
            channel_name = 'Left' if i == 0 else 'Right' if i == 1 else f'Channel {i+1}'
            color = 'red' if i == 0 else 'blue' if i == 1 else f'C{i}'
            axs[i+1].plot(time_axis, pcm_data[:, i], color=color, linewidth=LINEWIDTH)
            axs[i+1].set_title(f'{file_name} - {channel_name}')
            axs[i+1].set_xlabel('Time [s]')
            axs[i+1].set_ylabel('Amplitude')
            axs[i+1].grid(True)

    # Adjust layout to prevent overlap
    plt.tight_layout()
    plt.savefig(output_file)
    plt.close()
    
    return os.path.abspath(output_file)

def generate_waveform_plot(
    file_path1: str, 
    file_path2: Optional[str] = None,
    sample_rate: int = 48000, 
    output_file: str = 'waveform.png',
    num_channels1: int = 2,
    num_channels2: int = 2,
    downsample_factor: int = 10,
    start_time: Optional[float] = None,
    end_time: Optional[float] = None,
    pcm_format: int = 16
) -> str:
    """
    Generate a waveform plot from one or two PCM files.
    This function is designed to be called from tests or other code.
    
    Args:
        file_path1: Path to the first PCM file.
        file_path2: Path to the second PCM file (optional).
        sample_rate: Sample rate of the PCM files.
        output_file: Output file name for the waveform plot.
        num_channels1: Number of channels in the first PCM file.
        num_channels2: Number of channels in the second PCM file.
        downsample_factor: Factor by which to downsample the data.
        start_time: Start time in seconds for the plot.
        end_time: End time in seconds for the plot.
        pcm_format: PCM format (8, 16, or 24 bits).
        
    Returns:
        Absolute path to the generated output file.
    """
    pcm_data1 = read_pcm(file_path1, sample_rate, pcm_format=pcm_format, num_channels=num_channels1)
    
    if file_path2:
        pcm_data2 = read_pcm(file_path2, sample_rate, pcm_format=pcm_format, num_channels=num_channels2)
        return plot_waveforms(
            file_path1, file_path2, pcm_data1, pcm_data2, sample_rate, output_file, 
            downsample_factor=downsample_factor, start_time=start_time, end_time=end_time, 
            num_channels1=num_channels1, num_channels2=num_channels2
        )
    else:
        return plot_single_waveform(
            file_path1, pcm_data1, sample_rate, output_file, 
            downsample_factor=downsample_factor, start_time=start_time, end_time=end_time, 
            num_channels=num_channels1
        )

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Plot waveforms from PCM files.",
        epilog="""
        Example usage:
        python3 audio_graph.py /path/to/file1.pcm /path/to/file2.pcm --sample_rate 44100 --output_file output.png --num_channels1 1 --num_channels2 1 --downsample_factor 10 --start_time 0 --end_time 0.03 --pcm_format 16
        
        For single file:
        python3 audio_graph.py /path/to/file1.pcm --sample_rate 44100 --output_file output.png --num_channels1 2 --downsample_factor 10 --pcm_format 16
        """
    )
    parser.add_argument('file_path1', type=str, help='Path to the first PCM file.')
    parser.add_argument('file_path2', type=str, nargs='?', help='Path to the second PCM file (optional).')
    parser.add_argument('--sample_rate', type=int, default=48000, help='Sample rate of the PCM files.')
    parser.add_argument('--output_file', type=str, default='waveform.png', help='Output file name for the waveform plot.')
    parser.add_argument('--num_channels1', type=int, default=2, help='Number of channels in the first PCM file.')
    parser.add_argument('--num_channels2', type=int, default=2, help='Number of channels in the second PCM file.')
    parser.add_argument('--downsample_factor', type=int, default=10, help='Factor by which to downsample the data.')
    parser.add_argument('--start_time', type=float, help='Start time in seconds for the plot.')
    parser.add_argument('--end_time', type=float, help='End time in seconds for the plot.')
    parser.add_argument('--pcm_format', type=int, choices=[8, 16, 24], default=16, help='PCM format (8, 16, or 24 bits).')

    args = parser.parse_args()
    
    output_path = generate_waveform_plot(
        args.file_path1,
        args.file_path2,
        args.sample_rate,
        args.output_file,
        args.num_channels1,
        args.num_channels2,
        args.downsample_factor,
        args.start_time,
        args.end_time,
        args.pcm_format,
    )

    print(f"Waveform saved to {output_path}")