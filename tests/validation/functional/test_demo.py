import logging
import time

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import McmConnectionType
from common.ffmpeg_handler.ffmpeg_io import FFmpegVideoIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmMemifVideoIO
from common.integrity.integrity_runner import (
    FileVideoIntegrityRunner,
    StreamVideoIntegrityRunner,
)
from Engine.mcm_apps import MEDIA_PROXY_PORT


logger = logging.getLogger(__name__)


def test_ping_command(hosts):
    """Test executing a ping command on the SUT."""
    logger.info("Testing ping command")
    processes = []
    for host in hosts.values():
        process = host.connection.start_process("ping localhost")
        logger.info(process.running)
        processes.append(process)
    for process in processes:
        process.stop()
        logger.info(f"Ping output: {process.stdout_text}")
        assert "0% packet loss" in process.stdout_text


def test_fixtures_with_extra_data(extra_data):
    """Test using extra_data fixture to validate adapter information."""
    logger.info("Testing extra_data fixture")
    extra_data["tested_adapter"] = {
        "family": "CVL",
        "nvm": "80008812",
        "driver_version": "1.11.2",
    }
    logger.debug(f"Extra data: {extra_data}")
    assert extra_data["tested_adapter"]["family"] == "CVL"
    assert extra_data["tested_adapter"]["nvm"] == "80008812"
    assert extra_data["tested_adapter"]["driver_version"] == "1.11.2"


def test_print_configs(topology, test_config):
    """Test to print topology and test configuration."""
    logger.info("Testing configuration printing")
    logger.info(f"topology: {topology}")
    logger.info(f"test_config: {test_config}")
    assert topology, "Topology should not be empty"
    assert test_config, "Test configuration should not be empty"


def test_list_command_on_sut(hosts):
    """Test executing an 'ls' command on the SUT."""
    logger.info("Testing 'ls' command on SUT")
    logger.info(f"hosts: {hosts}")
    res = list(hosts.values())[0].connection.execute_command("ls", shell=True)
    logger.info(res.stdout)

    process = list(hosts.values())[0].connection.start_process("ping localhost")
    logger.info(process.running)
    process.stop()


def test_mesh_agent_lifecycle(mesh_agent):
    """Test starting and stopping the mesh agent."""
    logger.info("Testing mesh_agent lifecycle")
    assert (
        mesh_agent.mesh_agent_process is not None
    ), "Mesh agent process was not started."
    assert mesh_agent.mesh_agent_process.running, "Mesh agent process is not running."
    logger.info("Mesh agent lifecycle test completed successfully.")


def test_media_proxy(media_proxy):
    """Test starting and stopping the media proxy without sudo."""
    logger.info("Testing media_proxy lifecycle")
    for proxy in media_proxy.values():
        process = proxy.run_media_proxy_process
        assert process is not None, "Media proxy process was not started."
        assert process.running, "Media proxy process is not running."
    logger.info("Media proxy lifecycle test completed successfully.")


def test_sudo_command(hosts):
    """Test running a command with sudo using SSHConnection."""
    logger.info("Testing sudo command execution")
    connection = list(hosts.values())[0].connection
    # connection.enable_sudo()
    result = connection.execute_command("ls /", stderr_to_stdout=True)
    logger.info(f"Command output: {result.stdout}")
    logger.info(f"Command error (if any): {result.stderr}")
    assert (
        result.return_code == 0
    ), f"Sudo command failed with return code {result.return_code}"
    # connection.disable_sudo()
    logger.info("Sudo command execution test completed")


def test_demo_local_ffmpeg_video_integrity(media_proxy, hosts, test_config) -> None:
    # media_proxy fixture used only to ensure that the media proxy is running
    tx_host = rx_host = list(hosts.values())[0]
    prefix_variables = test_config.get("prefix_variables", {})
    if tx_host.topology.extra_info.media_proxy.get("no_proxy", None):
        prefix_variables["NO_PROXY"] = tx_host.topology.extra_info.media_proxy[
            "no_proxy"
        ]
        prefix_variables["no_proxy"] = tx_host.topology.extra_info.media_proxy[
            "no_proxy"
        ]
    sdk_port = MEDIA_PROXY_PORT
    if tx_host.name in media_proxy and media_proxy[tx_host.name].p is not None:
        sdk_port = media_proxy[tx_host.name].t
    prefix_variables["MCM_MEDIA_PROXY_PORT"] = sdk_port

    frame_rate = "60"
    video_size = "1920x1080"
    pixel_format = "yuv422p10le"
    conn_type = McmConnectionType.mpg.value

    input_path = str(
        tx_host.connection.path(
            test_config["input_path"], "180fr_1920x1080_yuv422p.yuv"
        )
    )

    # >>>>> MCM Tx
    mcm_tx_inp = FFmpegVideoIO(
        framerate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        stream_loop=False,
        input_path=input_path,
    )
    mcm_tx_outp = FFmpegMcmMemifVideoIO(
        f="mcm",
        conn_type=conn_type,
        frame_rate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        output_path="-",
    )
    mcm_tx_ff = FFmpeg(
        prefix_variables=prefix_variables,
        ffmpeg_path=test_config["ffmpeg_path"],
        ffmpeg_input=mcm_tx_inp,
        ffmpeg_output=mcm_tx_outp,
        yes_overwrite=False,
    )

    logger.debug(f"Tx command: {mcm_tx_ff.get_command()}")
    mcm_tx_executor = FFmpegExecutor(tx_host, ffmpeg_instance=mcm_tx_ff)

    # >>>>> MCM Rx
    mcm_rx_inp = FFmpegMcmMemifVideoIO(
        f="mcm",
        conn_type=conn_type,
        frame_rate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        input_path="-",
    )
    mcm_rx_outp = FFmpegVideoIO(
        f="rawvideo",
        framerate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        output_path=str(
            tx_host.connection.path(test_config["output_path"], "output_vid.yuv")
        ),
    )
    mcm_rx_ff = FFmpeg(
        prefix_variables=prefix_variables,
        ffmpeg_path=test_config["ffmpeg_path"],
        ffmpeg_input=mcm_rx_inp,
        ffmpeg_output=mcm_rx_outp,
        yes_overwrite=True,
    )

    logger.debug(f"Rx command: {mcm_rx_ff.get_command()}")
    mcm_rx_executor = FFmpegExecutor(rx_host, ffmpeg_instance=mcm_rx_ff)

    integrator = FileVideoIntegrityRunner(
        host=rx_host,
        test_repo_path=None,  # mcm_path when we'll have tests in mcm repo
        src_url=input_path,
        out_name="output_vid.yuv",
        resolution=video_size,
        file_format=pixel_format,
        out_path=str(rx_host.connection.path(test_config["output_path"])),
        delete_file=False,
        integrity_path="/opt/intel/val_tests/validation/common/integrity/",  # Path to the integrity script, if don't want to use the default one from mcm_repo
    )
    integrator.setup()
    mcm_rx_executor.start()
    time.sleep(3)  # Ensure the receiver is ready before starting the transmitter
    mcm_tx_executor.start()
    mcm_rx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
    mcm_tx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
    result = integrator.run()
    assert result, "Integrity check failed"


def test_demo_local_ffmpeg_video_stream(media_proxy, hosts, test_config) -> None:
    # media_proxy fixture used only to ensure that the media proxy is running
    tx_host = rx_host = list(hosts.values())[0]
    prefix_variables = test_config.get("prefix_variables", {})
    if tx_host.topology.extra_info.media_proxy.get("no_proxy", None):
        prefix_variables["NO_PROXY"] = tx_host.topology.extra_info.media_proxy[
            "no_proxy"
        ]
        prefix_variables["no_proxy"] = tx_host.topology.extra_info.media_proxy[
            "no_proxy"
        ]
    sdk_port = MEDIA_PROXY_PORT
    if tx_host.name in media_proxy and media_proxy[tx_host.name].p is not None:
        sdk_port = media_proxy[tx_host.name].t
    prefix_variables["MCM_MEDIA_PROXY_PORT"] = sdk_port

    frame_rate = "60"
    video_size = "1920x1080"
    pixel_format = "yuv422p10le"
    conn_type = McmConnectionType.mpg.value

    input_path = str(
        tx_host.connection.path(
            test_config["input_path"], "180fr_1920x1080_yuv422p.yuv"
        )
    )

    # >>>>> MCM Tx
    mcm_tx_inp = FFmpegVideoIO(
        read_at_native_rate=True,  # Keep reading with given framerate
        framerate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        stream_loop=True,
        input_path=input_path,
    )
    mcm_tx_outp = FFmpegMcmMemifVideoIO(
        f="mcm",
        conn_type=conn_type,
        frame_rate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        output_path="-",
    )
    mcm_tx_ff = FFmpeg(
        prefix_variables=prefix_variables,
        ffmpeg_path=test_config["ffmpeg_path"],
        ffmpeg_input=mcm_tx_inp,
        ffmpeg_output=mcm_tx_outp,
        yes_overwrite=False,
    )

    logger.debug(f"Tx command: {mcm_tx_ff.get_command()}")
    mcm_tx_executor = FFmpegExecutor(tx_host, ffmpeg_instance=mcm_tx_ff)

    # >>>>> MCM Rx
    mcm_rx_inp = FFmpegMcmMemifVideoIO(
        f="mcm",
        conn_type=conn_type,
        frame_rate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        input_path="-",
    )
    mcm_rx_outp = FFmpegVideoIO(
        f="rawvideo",
        segment=3,  # Segment the output every 3 seconds
        framerate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        output_path=str(
            tx_host.connection.path(test_config["output_path"], "output_vid_%03d.yuv")
        ),  # add segmenting pattern into the filename
    )
    mcm_rx_ff = FFmpeg(
        prefix_variables=prefix_variables,
        ffmpeg_path=test_config["ffmpeg_path"],
        ffmpeg_input=mcm_rx_inp,
        ffmpeg_output=mcm_rx_outp,
        yes_overwrite=True,
    )

    logger.debug(f"Rx command: {mcm_rx_ff.get_command()}")
    mcm_rx_executor = FFmpegExecutor(rx_host, ffmpeg_instance=mcm_rx_ff)

    integrator = StreamVideoIntegrityRunner(
        host=rx_host,
        test_repo_path=None,  # mcm_path when we'll have tests in mcm repo
        src_url=input_path,
        out_name="output_vid",
        resolution=video_size,
        file_format=pixel_format,
        out_path=str(rx_host.connection.path(test_config["output_path"])),
        delete_file=True,
        integrity_path="/opt/intel/val_tests/validation/common/integrity/",  # Path to the integrity script, if don't want to use the default one from mcm_repo
    )
    integrator.setup()
    integrator.run()  # Start the integrity check in the background - it can be started before the traffic starts it will wait for files to check
    mcm_rx_executor.start()
    time.sleep(3)  # Ensure the receiver is ready before starting the transmitter
    mcm_tx_executor.start()
    mcm_rx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
    mcm_tx_executor.stop(
        wait=3
    )  # Tx should stop just after Rx stop so wait timeout can be shorter here

    assert integrator.stop_and_verify(timeout=20), "Stream integrity check failed"


def test_build_mcm_ffmpeg(build_mcm_ffmpeg, hosts):
    """Test the MCM FFmpeg build process."""
    logger.info("Testing MCM FFmpeg build process")
    assert build_mcm_ffmpeg, "MCM FFmpeg build failed"


def test_build_mtl_ffmpeg(build_mtl_ffmpeg, hosts):
    """Test the MTL FFmpeg build process."""
    logger.info("Testing MTL FFmpeg build process")
    assert build_mtl_ffmpeg, "MTL FFmpeg build failed"