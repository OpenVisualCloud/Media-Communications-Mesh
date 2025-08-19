import logging
import time

import pytest

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import McmConnectionType
from common.ffmpeg_handler.ffmpeg_io import FFmpegVideoIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmMemifVideoIO
from common.integrity.integrity_runner import (
    FileVideoIntegrityRunner,
    StreamVideoIntegrityRunner,
    FileBlobIntegrityRunner,
)
from Engine.mcm_apps import MEDIA_PROXY_PORT
import Engine.rx_tx_app_connection
import Engine.rx_tx_app_engine_mcm as utils
import Engine.rx_tx_app_payload
from Engine.const import (
    DEFAULT_LOOP_COUNT,
    MCM_ESTABLISH_TIMEOUT,
    MCM_RXTXAPP_RUN_TIMEOUT,
)
from Engine.media_files import blob_files_25_03


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


@pytest.mark.smoke
@pytest.mark.parametrize("logging", ["logging_on"])
def test_mesh_agent_lifecycle(mesh_agent, logging):
    """Test starting and stopping the mesh agent."""
    logger.info("Testing mesh_agent lifecycle")
    assert (
        mesh_agent.mesh_agent_process is not None
    ), "Mesh agent process was not started."
    assert mesh_agent.mesh_agent_process.running, "Mesh agent process is not running."
    logger.info("Mesh agent lifecycle test completed successfully.")


@pytest.mark.smoke
@pytest.mark.parametrize("logging", ["logging_on"])
def test_media_proxy(media_proxy, logging):
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


def test_demo_local_blob_integrity(
    build_TestApp, hosts, media_proxy, media_path, log_path
) -> None:
    """Test blob integrity checking with a single blob file transfer using LapkaExecutor."""
    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 1:
        pytest.skip("Local tests require at least 1 host")
    tx_host = rx_host = host_list[0]

    # Use the same blob file configuration as the actual blob test
    blob_file_key = "random_bin_100M"
    file_dict = blob_files_25_03[blob_file_key]

    tx_executor = utils.LapkaExecutor.Tx(
        host=tx_host,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.MultipointGroup,
        payload_type=Engine.rx_tx_app_payload.Blob,
        file_dict=file_dict,
        file=blob_file_key,
        loop=1,  # Use single loop for integrity testing
        log_path=log_path,
    )
    rx_executor = utils.LapkaExecutor.Rx(
        host=rx_host,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.MultipointGroup,
        payload_type=Engine.rx_tx_app_payload.Blob,
        file_dict=file_dict,
        file=blob_file_key,
        log_path=log_path,
    )

    # Debug: Log the paths before starting
    logger.info(f"TX input path: {tx_executor.input}")
    logger.info(f"RX output path: {rx_executor.output}")
    logger.info(f"Output directory: {rx_executor.output.parent}")
    logger.info(f"Output filename: {rx_executor.output.name}")

    # Start processes following the same pattern as blob test
    rx_executor.start()
    time.sleep(MCM_ESTABLISH_TIMEOUT)
    tx_executor.start()

    try:
        if rx_executor.process.running:
            rx_executor.process.wait(timeout=MCM_RXTXAPP_RUN_TIMEOUT)
    except Exception as e:
        logging.warning(f"RX executor did not finish in time or error occurred: {e}")

    tx_executor.stop()
    rx_executor.stop()

    assert tx_executor.is_pass is True, "TX process did not pass"
    assert rx_executor.is_pass is True, "RX process did not pass"

    # Check if the output file actually exists
    output_exists = (
        rx_host.connection.execute_command(
            f"test -f {rx_executor.output}", shell=True
        ).return_code
        == 0
    )
    logger.info(f"Output file exists: {output_exists}")

    if output_exists:
        # Get actual file size for debugging
        file_size_result = rx_host.connection.execute_command(
            f"stat -c%s {rx_executor.output}", shell=True
        )
        logger.info(f"Output file size: {file_size_result.stdout.strip()} bytes")

        # Setup blob integrity checker
        integrator = FileBlobIntegrityRunner(
            host=rx_host,
            test_repo_path=None,
            src_url=str(tx_executor.input),  # Use the actual input path from executor
            out_name=rx_executor.output.name,  # Use the actual output filename
            chunk_size=int(
                file_dict["max_payload_size"]
            ),  # Use payload size as chunk size
            out_path=str(rx_executor.output.parent),  # Use the output directory
            delete_file=False,
            integrity_path="/opt/intel/val_tests/validation/common/integrity/",
        )

        integrator.setup()

        # Run integrity check with better error handling
        try:
            result = integrator.run()
            logger.info(f"Integrity check result: {result}")
        except Exception as e:
            logger.error(f"Integrity check failed with exception: {e}")
            result = False

        if not result:
            # Get more details about what went wrong
            logger.error("=== Debugging integrity check failure ===")

            # Check source file details
            src_exists = (
                tx_host.connection.execute_command(
                    f"test -f {tx_executor.input}", shell=True
                ).return_code
                == 0
            )
            if src_exists:
                src_size_result = tx_host.connection.execute_command(
                    f"stat -c%s {tx_executor.input}", shell=True
                )
                logger.error(
                    f"Source file size: {src_size_result.stdout.strip()} bytes"
                )
            else:
                logger.error(f"Source file {tx_executor.input} does not exist")

            # Check output file details again
            out_size_result = rx_host.connection.execute_command(
                f"stat -c%s {rx_executor.output}", shell=True
            )
            logger.error(f"Output file size: {out_size_result.stdout.strip()} bytes")

            # Compare sizes
            logger.error(
                f"Max payload size (chunk size): {file_dict['max_payload_size']}"
            )

            # Try to get some hex dump of both files for comparison
            logger.error("First 32 bytes of source file:")
            src_hex = tx_host.connection.execute_command(
                f"hexdump -C {tx_executor.input} | head -3", shell=True
            )
            logger.error(src_hex.stdout)

            logger.error("First 32 bytes of output file:")
            out_hex = rx_host.connection.execute_command(
                f"hexdump -C {rx_executor.output} | head -3", shell=True
            )
            logger.error(out_hex.stdout)

        assert result, "Blob integrity check failed"
    else:
        # List files in the output directory to see what was actually created
        ls_result = rx_host.connection.execute_command(
            f"ls -la {rx_executor.output.parent}/", shell=True
        )
        logger.error(f"Output directory contents:\n{ls_result.stdout}")

        # Find any files that might match our pattern
        find_result = rx_host.connection.execute_command(
            f"find {rx_executor.output.parent}/ -name '*random*' -o -name '*rx_*' -o -name '*blob*'",
            shell=True,
        )
        logger.error(f"Potential output files:\n{find_result.stdout}")

        assert False, f"Output file {rx_executor.output} was not created"


def test_build_mcm_ffmpeg(build_mcm_ffmpeg, hosts, test_config):
    """
    Test the MCM FFmpeg build process.
    mcm_ffmpeg_rebuild needs to be set to True.
    """
    logger.info("Testing MCM FFmpeg build process")
    assert build_mcm_ffmpeg, "MCM FFmpeg build failed"


def test_build_mtl_ffmpeg(build_mtl_ffmpeg, hosts, test_config):
    """
    Test the MTL FFmpeg build process.
    mtl_ffmpeg_rebuild needs to be set to True.
    """
    logger.info("Testing MTL FFmpeg build process")
    assert build_mtl_ffmpeg, "MTL FFmpeg build failed"

def test_simple(log_path_dir, log_path, request):
    # For this test, log_path will be based on "test_simple"
    logging.info(f"Log path dir for test_simple: {log_path_dir}")
    logging.info(f"Log path for test_simple: {log_path}")
    logging.info(f"Request: {request.node.name}")