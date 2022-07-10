import zipfile
import json
from cobble import cobble
from time import sleep
from enum import IntEnum
from binascii import crc32

# Implementation of Nordic Secure BLE using Cobble
# Legacy has subtle differences.

dfu_sv_uuid = "0000FE59-0000-1000-8000-00805F9B34FB"
dfu_ctrl_uuid = "8EC90001-F315-4F60-9FB8-838830DAEA50"
dfu_data_uuid = "8EC90002-F315-4F60-9FB8-838830DAEA50"

class NRF_DFU_OBJ_TYPE(IntEnum):
    INVALID = 0,                   #!< Invalid object type.
    COMMAND = 1,                   #!< Command object.
    DATA = 2                       #!< Data object.

class NRF_DFU_OP(IntEnum):
    PROTOCOL_VERSION     = 0x00      #!< Retrieve protocol version.
    OBJECT_CREATE        = 0x01      #!< Create selected object.
    RECEIPT_NOTIF_SET    = 0x02      #!< Set receipt notification.
    CRC_GET              = 0x03      #!< Request CRC of selected object.
    OBJECT_EXECUTE       = 0x04      #!< Execute selected object.
    OBJECT_SELECT        = 0x06      #!< Select object.
    MTU_GET              = 0x07      #!< Retrieve MTU size.
    OBJECT_WRITE         = 0x08      #!< Write selected object. DATA characteristic handles this automatically, no need to add the opcode, just write data
    PING                 = 0x09      #!< Ping.
    HARDWARE_VERSION     = 0x0A      #!< Retrieve hardware version.
    FIRMWARE_VERSION     = 0x0B      #!< Retrieve firmware version.
    ABORT                = 0x0C      #!< Abort the DFU procedure.
    RESPONSE             = 0x60      #!< Response.
    INVALID              = 0xFF,

class NRF_DFU_RES_CODE(IntEnum):
    INVALID                 = 0x00     #!< Invalid opcode.
    SUCCESS                 = 0x01     #!< Operation successful.
    OP_CODE_NOT_SUPPORTED   = 0x02     #!< Opcode not supported.
    INVALID_PARAMETER       = 0x03     #!< Missing or invalid parameter value.
    INSUFFICIENT_RESOURCES  = 0x04     #!< Not enough memory for the data object.
    INVALID_OBJECT          = 0x05     #!< Data object does not match the firmware and hardware requirements, the signature is wrong, or parsing the command failed.
    UNSUPPORTED_TYPE        = 0x07     #!< Not a valid object type for a Create request.
    OPERATION_NOT_PERMITTED = 0x08     #!< The state of the DFU process does not allow this operation.
    OPERATION_FAILED        = 0x0A     #!< Operation failed.
    EXT_ERROR               = 0x0B     #!< Extended error. The next byte of the response contains the error code of the extended error (see @ref code_t.

class NRF_DFU_FIRMWARE_TYPE(IntEnum):
    SOFTDEVICE    = 0x00
    APPLICATION   = 0x01
    BOOTLOADER    = 0x02
    UNKNOWN       = 0xFF

class NRF_DFU_EXT_ERROR(IntEnum):
    NO_ERROR                  = 0x00     # No extended error code has been set. This error indicates an implementation problem. */
    INVALID_ERROR_CODE        = 0x01     # Invalid error code. This error code should never be used outside of development. */
    WRONG_COMMAND_FORMAT      = 0x02     # The format of the command was incorrect. This error code is not used in the
                                         #                    current implementation, because @ref NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED
                                         #                    and @ref NRF_DFU_RES_CODE_INVALID_PARAMETER cover all
                                         #                    possible format errors. */
    UNKNOWN_COMMAND           = 0x03     # The command was successfully parsed, but it is not supported or unknown. */
    INIT_COMMAND_INVALID      = 0x04     # The init command is invalid. The init packet either has
                                         #                    an invalid update type or it is missing required fields for the update type
                                         #                    (for example, the init packet for a SoftDevice update is missing the SoftDevice size field). */
    FW_VERSION_FAILURE        = 0x05     # The firmware version is too low. For an application or SoftDevice, the version must be greater than
                                         #                    or equal to the current version. For a bootloader, it must be greater than the current version.
                                         #                    to the current version. This requirement prevents downgrade attacks.*/
    HW_VERSION_FAILURE        = 0x06     # The hardware version of the device does not match the required
                                         #                    hardware version for the update. */
    SD_VERSION_FAILURE        = 0x07     # The array of supported SoftDevices for the update does not contain
                                         #                    the FWID of the current SoftDevice or the first FWID is '0' on a
                                         #                    bootloader which requires the SoftDevice to be present. */
    SIGNATURE_MISSING         = 0x08     # The init packet does not contain a signature. This error code is not used in the
                                         #                    current implementation, because init packets without a signature
                                         #                    are regarded as invalid. */
    WRONG_HASH_TYPE           = 0x09     # The hash type that is specified by the init packet is not supported by the DFU bootloader. */
    HASH_FAILED               = 0x0A     # The hash of the firmware image cannot be calculated. */
    WRONG_SIGNATURE_TYPE      = 0x0B     # The type of the signature is unknown or not supported by the DFU bootloader. */
    VERIFICATION_FAILED       = 0x0C     # The hash of the received firmware image does not match the hash in the init packet. */
    INSUFFICIENT_SPACE        = 0x0D     # The available space on the device is insufficient to hold the firmware. */


def load_firmware(fname):
    with zipfile.ZipFile(fname) as zf:
        with zf.open("manifest.json") as mf:
            js = json.loads(mf.read())
            bin_file = zf.open(js['manifest']['application']['bin_file'], 'r').read()
            dat_file = zf.open(js['manifest']['application']['dat_file'], 'r').read()

    print("Loaded FW OK")
    return (bin_file, dat_file)


def set_data(data, chunk_size=244):
    # When reviewing bootloader code - note that these are still handled 
    # They just have the type NRF_DFU_OP_OBJECT_WRITE appended automatically

    bytes_sent = 0
    print(f"Sending {len(data)} bytes in {chunk_size} byte chunks")
    while(bytes_sent < len(data)):
        # Take the next chunk of data
        next_size = min(chunk_size, len(data)-bytes_sent)
        segment = data[bytes_sent : bytes_sent + next_size]
        cobble.write(dfu_data_uuid, segment)
        sleep(0.1)
        bytes_sent += next_size
        print(f"Sent {bytes_sent} of {len(data)} bytes...")
    print("Transfer complete")


def set_control(data):
    cobble.write(dfu_ctrl_uuid, bytes(data))
    
def expect_response():

    # Await a notification
    while (notif := cobble.get_updatevalue()) == None:
        sleep(0.01)

    # Response data
    rd = notif[1]

    # Confirm that it's a response to a command
    op = NRF_DFU_OP(rd[0])
    assert op == NRF_DFU_OP.RESPONSE, f"Expecting a response but got operation code {op}:{op.name}"

    # Check that we're dealing with a command we understand
    cmd = NRF_DFU_OP(rd[1])

    # Confirm that it's a success
    res = NRF_DFU_RES_CODE(rd[2])

    # Handle errors
    if(res == NRF_DFU_RES_CODE.EXT_ERROR):
        ext = NRF_DFU_EXT_ERROR(rd[3])
        assert False, f"Device reported extended error {ext}:{ext.name}"
    assert res == NRF_DFU_RES_CODE.SUCCESS, f"Device replied with an error response {res}:{res.name}"

    return rd
    

def do_legacy_update(fname, identifier):
    # TODO: Implement
    pass

def u32le(i):
    return ((i[0]<<0) | (i[1]<<8) | (i[2]<<16) | (i[3]<<24))

def u32tole(i):
    return [(i & 0xFF), ((i >> 8) & 0xFF), ((i >> 16) & 0xFF), ((i >> 24) & 0xFF)]

def do_secure_update(fname, identifier):

    bin_file, dat_file = load_firmware(fname)

    cobble.connect(identifier)
    # TODO: Await connection event rather than just sleeping
    sleep(2)

    assert len(cobble.characteristics) > 0, "Failed to find DFU characteristics in time"

    assert (dfu_sv_uuid, dfu_ctrl_uuid) in cobble.characteristics, "Missing DFU Control characteristic"
    assert (dfu_sv_uuid, dfu_data_uuid) in cobble.characteristics, "Missing DFU Packet characteristic"

    # DFU Process
    print("Subscribing...")

    # Listen for changes on the DFU Control characteristic
    cobble.subscribe(dfu_ctrl_uuid)
    sleep(4)

    print("Initialising DFU...")

    # TODO: For speed, request maximum MTU?  MTU 247 (244 usable bytes) better than MTU 23 (20 usable bytes)
    # TODO: Exchange any information we might be interested in.

    # Flow as in:
    # https:#infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.sdk5.v14.1.0%2Flib_bootloader_dfu_process.html

    # Select the init packet, see if it matches our expectations
    set_control(bytes([NRF_DFU_OP.OBJECT_SELECT, NRF_DFU_OBJ_TYPE.COMMAND]))
    rd = expect_response()
    maxSize = u32le(rd[3:7])
    offset = u32le(rd[7:11])
    crc = u32le(rd[11:15])

    # Are the values what we'd expect? If so, we're resuming a faulty transfer
    if(crc != crc32(dat_file)) or (offset != len(dat_file)):

        # Tell the DFU to expect a new init packet
        initlen = u32tole(len(dat_file))
        set_control(bytes([NRF_DFU_OP.OBJECT_CREATE, NRF_DFU_OBJ_TYPE.COMMAND, initlen[0], initlen[1], initlen[2], initlen[3]]))
        rd = expect_response()

        # Send the init data
        set_data(dat_file)

        # CRC it
        set_control(bytes([NRF_DFU_OP.CRC_GET]))
        rd = expect_response()
        assert NRF_DFU_OP(rd[1]) == NRF_DFU_OP.CRC_GET, "Received weird response"
        offset = u32le(rd[3:7])
        crc = u32le(rd[7:11])
        assert offset == len(dat_file), "Device received fewer bytes than we sent"
        assert crc == crc32(dat_file), f"CRC check failed: Our file is {crc32(dat_file)}, but device got {crc}"

        # Prevalidate (execute)
        set_control(bytes([NRF_DFU_OP.OBJECT_EXECUTE]))
        rd = expect_response()

    # Device is now stuck in DFU mode until an update completes.
    print("Initialised OK, now sending data...")

    # Select the data packet
    set_control(bytes([NRF_DFU_OP.OBJECT_SELECT, NRF_DFU_OBJ_TYPE.DATA]))
    rd = expect_response()
    maxSize = u32le(rd[3:7])
    offset = u32le(rd[7:11])
    crc = u32le(rd[11:15])

    assert offset==0, "Resuming not supported!"

    # The device can't accept the entire firmware in one transfer, it needs chunks (in the case of nRF52832, 4096 bytes max)
    # Smaller chunks also gives us more opportunities to catch issues with the CRC and retry without needing to
    # re-send the whole image.
    num_data_objects = ((len(bin_file) + maxSize - 1) // maxSize)
    print(f"We will send {num_data_objects} data objects of size {maxSize} to transfer the needed {len(bin_file)} bytes")

    bytes_remaining = len(bin_file)

    while(bytes_remaining):

        print(f"Bytes remaining: {bytes_remaining}")
        # TODO: Can we CRC this block without writing, to save time if resuming a disrupted upload?
        
        transfer_size = min(bytes_remaining, maxSize)
        data_start_idx = len(bin_file) - bytes_remaining
        data_end_idx = data_start_idx + transfer_size
        chunk = bin_file[data_start_idx : data_end_idx]

        # Create data object
        datalen = u32tole(transfer_size)
        set_control(bytes([NRF_DFU_OP.OBJECT_CREATE, NRF_DFU_OBJ_TYPE.DATA, datalen[0], datalen[1], datalen[2], datalen[3]]))
        rd = expect_response()

        # Transfer firmware data
        set_data(chunk)

        # CRC
        set_control(bytes([NRF_DFU_OP.CRC_GET]))
        rd = expect_response()
        assert NRF_DFU_OP(rd[1]) == NRF_DFU_OP.CRC_GET, "Received weird response"
        offset = u32le(rd[3:7])
        crc = u32le(rd[7:11])
        expected_offset = len(bin_file) - bytes_remaining + transfer_size
        expected_crc = crc32(bin_file[:data_end_idx])
        assert offset == expected_offset, f"Device thinks offset is {offset}, we think {expected_offset}"

        # Execute
        set_control(bytes([NRF_DFU_OP.OBJECT_EXECUTE]))
        rd = expect_response()

        bytes_remaining -= transfer_size

    # Postvalidate and boot happens automatically on completion


    print("DONE")

