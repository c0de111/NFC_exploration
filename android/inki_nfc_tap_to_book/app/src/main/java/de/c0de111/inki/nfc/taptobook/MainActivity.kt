package de.c0de111.inki.nfc.taptobook

import android.content.Intent
import android.nfc.NfcAdapter
import android.nfc.Tag
import android.nfc.tech.NfcV
import android.os.Build
import android.os.Bundle
import android.view.HapticFeedbackConstants
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import java.io.IOException
import java.security.SecureRandom
import kotlin.math.ceil

class MainActivity : AppCompatActivity() {
    private companion object {
        const val OPCODE_LED1_SLOW = 0x11
        const val OPCODE_LED2_FAST = 0x12
    }

    private enum class WriteState {
        READY,
        WRITING,
        DONE,
        FAILED
    }

    private var nfcAdapter: NfcAdapter? = null

    private lateinit var logView: TextView
    private lateinit var commandView: TextView
    private lateinit var writeStateView: TextView
    @Volatile private var selectedOpcode: Int = OPCODE_LED1_SLOW
    @Volatile private var writeInProgress: Boolean = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        logView = findViewById(R.id.tvLog)
        commandView = findViewById(R.id.tvCommand)
        writeStateView = findViewById(R.id.tvWriteState)

        findViewById<Button>(R.id.btnCmdLed1Slow).setOnClickListener {
            selectCommand(OPCODE_LED1_SLOW)
            log("Selected command: ${opcodeLabel(selectedOpcode)}")
        }
        findViewById<Button>(R.id.btnCmdLed2Fast).setOnClickListener {
            selectCommand(OPCODE_LED2_FAST)
            log("Selected command: ${opcodeLabel(selectedOpcode)}")
        }

        findViewById<Button>(R.id.btnClearLog).setOnClickListener {
            logView.text = ""
        }

        selectCommand(OPCODE_LED1_SLOW)
        setWriteState(WriteState.READY)

        nfcAdapter = NfcAdapter.getDefaultAdapter(this)
        if (nfcAdapter == null) {
            log("No NFC adapter found")
        }

        handleNfcIntent(intent)
    }

    override fun onResume() {
        super.onResume()
        // Reader mode makes it easy to test by opening the app first and tapping the tag.
        nfcAdapter?.enableReaderMode(
            this,
            { tag -> onTagDiscovered(tag) },
            NfcAdapter.FLAG_READER_NFC_V or NfcAdapter.FLAG_READER_SKIP_NDEF_CHECK,
            null
        )
    }

    override fun onPause() {
        super.onPause()
        nfcAdapter?.disableReaderMode(this)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleNfcIntent(intent)
    }

    private fun handleNfcIntent(intent: Intent) {
        val action = intent.action ?: return
        if (action != NfcAdapter.ACTION_TECH_DISCOVERED && action != NfcAdapter.ACTION_TAG_DISCOVERED) {
            return
        }

        val tag = getTagExtra(intent) ?: return
        onTagDiscovered(tag)
    }

    private fun onTagDiscovered(tag: Tag) {
        if (writeInProgress) {
            log("Busy: still processing previous tap")
            return
        }
        writeInProgress = true
        setWriteState(WriteState.WRITING)

        val uid = tag.id
        log("\nTag discovered")
        log("  techs=${tag.techList.joinToString()}")
        log("  uid=${uid.toHex()} (Tag.getId)")

        val nfcv = NfcV.get(tag)
        if (nfcv == null) {
            log("No NfcV on this tag")
            setWriteState(WriteState.FAILED)
            signalFailureHaptic()
            writeInProgress = false
            return
        }

        Thread {
            var writeSucceeded = false
            try {
                nfcv.connect()

                val sys = tryGetSystemInfo(nfcv, uid)
                val bytesPerBlock = sys?.bytesPerBlock ?: 4
                val numBlocks = sys?.numBlocks ?: 128
                log("SystemInfo: numBlocks=$numBlocks bytesPerBlock=$bytesPerBlock")

                val opcode = selectedOpcode
                val payload = buildBookingRequest(opcode = opcode, durationMinutes = 60)
                val blocksNeeded = ceil(payload.size / bytesPerBlock.toDouble()).toInt()
                val startBlock = (numBlocks - blocksNeeded).coerceAtLeast(0)

                log("Command: ${opcodeLabel(opcode)} (0x${"%02X".format(opcode)})")
                log("Request: ${payload.toHex()}")
                log("Target: blocks $startBlock..${startBlock + blocksNeeded - 1}")

                writePayload(nfcv, uid, startBlock, bytesPerBlock, payload)
                log("Write: OK")

                val readBack = readPayload(nfcv, uid, startBlock, bytesPerBlock, blocksNeeded)
                val readBackPayload = readBack.copyOf(payload.size)
                log("ReadBack: ${readBackPayload.toHex()}")

                writeSucceeded = readBackPayload.contentEquals(payload)
                if (!writeSucceeded) {
                    log("Verify: mismatch (No Success - Try again!)")
                }

            } catch (e: Exception) {
                log("ERROR: ${e.javaClass.simpleName}: ${e.message}")
            } finally {
                try {
                    nfcv.close()
                } catch (_: Exception) {
                }
                if (writeSucceeded) {
                    setWriteState(WriteState.DONE)
                    signalSuccessHaptic()
                } else {
                    setWriteState(WriteState.FAILED)
                    signalFailureHaptic()
                }
                writeInProgress = false
            }
        }.start()
    }

    private fun writePayload(
        nfcv: NfcV,
        uid: ByteArray,
        startBlock: Int,
        bytesPerBlock: Int,
        payload: ByteArray
    ) {
        val blocksNeeded = ceil(payload.size / bytesPerBlock.toDouble()).toInt()
        for (i in 0 until blocksNeeded) {
            val block = startBlock + i
            val sliceStart = i * bytesPerBlock
            val sliceEnd = minOf((i + 1) * bytesPerBlock, payload.size)
            val blockData = ByteArray(bytesPerBlock)
            System.arraycopy(payload, sliceStart, blockData, 0, sliceEnd - sliceStart)
            writeSingleBlock(nfcv, uid, block, blockData)
        }
    }

    private fun readPayload(
        nfcv: NfcV,
        uid: ByteArray,
        startBlock: Int,
        bytesPerBlock: Int,
        blocksNeeded: Int
    ): ByteArray {
        val out = ByteArray(blocksNeeded * bytesPerBlock)
        for (i in 0 until blocksNeeded) {
            val block = startBlock + i
            val data = readSingleBlock(nfcv, uid, block)
            if (data.size < bytesPerBlock) {
                throw IOException("Short read on block $block: got ${data.size}")
            }
            System.arraycopy(data, 0, out, i * bytesPerBlock, bytesPerBlock)
        }
        return out
    }

    private data class SystemInfo(
        val uid: ByteArray,
        val numBlocks: Int?,
        val bytesPerBlock: Int?
    )

    private fun tryGetSystemInfo(nfcv: NfcV, uid: ByteArray): SystemInfo? {
        // ISO15693 Get System Info (0x2B)
        return try {
            val resp = transceiveChecked(nfcv, iso15693Cmd(uid, 0x2B.toByte(), byteArrayOf()))
            // resp[0]=flags, resp[1]=infoFlags, resp[2..9]=uid, then optional fields
            if (resp.size < 10) return null
            val infoFlags = resp[1].toInt() and 0xFF
            var idx = 2
            val uidFromTag = resp.copyOfRange(idx, idx + 8)
            idx += 8

            if (infoFlags and 0x01 != 0) idx += 1 // DSFID
            if (infoFlags and 0x02 != 0) idx += 1 // AFI

            var numBlocks: Int? = null
            var bytesPerBlock: Int? = null
            if (infoFlags and 0x04 != 0) {
                if (resp.size >= idx + 2) {
                    numBlocks = (resp[idx].toInt() and 0xFF) + 1
                    bytesPerBlock = (resp[idx + 1].toInt() and 0xFF) + 1
                }
                idx += 2
            }

            SystemInfo(uidFromTag, numBlocks, bytesPerBlock)
        } catch (_: Exception) {
            null
        }
    }

    private fun readSingleBlock(nfcv: NfcV, uid: ByteArray, block: Int): ByteArray {
        // ISO15693 Read Single Block (0x20)
        val resp = transceiveChecked(
            nfcv,
            iso15693Cmd(uid, 0x20.toByte(), byteArrayOf(block.toByte()))
        )
        // resp[0]=flags, data follows
        return resp.copyOfRange(1, resp.size)
    }

    private fun writeSingleBlock(nfcv: NfcV, uid: ByteArray, block: Int, data: ByteArray) {
        // ISO15693 Write Single Block (0x21)
        val params = ByteArray(1 + data.size)
        params[0] = block.toByte()
        System.arraycopy(data, 0, params, 1, data.size)
        transceiveChecked(nfcv, iso15693Cmd(uid, 0x21.toByte(), params))
    }

    private fun iso15693Cmd(uid: ByteArray, command: Byte, params: ByteArray): ByteArray {
        // Flags: addressed (0x20) + high data rate (0x02) = 0x22
        if (uid.size != 8) throw IOException("Unexpected UID length: ${uid.size}")

        val flags = 0x22.toByte()
        val out = ByteArray(2 + 8 + params.size)
        out[0] = flags
        out[1] = command
        System.arraycopy(uid, 0, out, 2, 8)
        System.arraycopy(params, 0, out, 10, params.size)
        return out
    }

    private fun transceiveChecked(nfcv: NfcV, cmd: ByteArray): ByteArray {
        val resp = nfcv.transceive(cmd)
        if (resp.isEmpty()) throw IOException("Empty response")

        val flags = resp[0].toInt() and 0xFF
        val isError = (flags and 0x01) != 0
        if (isError) {
            val code = if (resp.size > 1) resp[1].toInt() and 0xFF else -1
            throw IOException("Tag error: 0x${code.toString(16).padStart(2, '0')}")
        }

        return resp
    }

    private fun buildBookingRequest(opcode: Int, durationMinutes: Int): ByteArray {
        val out = ByteArray(16)
        out[0] = 'I'.code.toByte()
        out[1] = 'N'.code.toByte()
        out[2] = 'K'.code.toByte()
        out[3] = 'I'.code.toByte()
        out[4] = 0x01
        out[5] = (opcode and 0xFF).toByte()
        out[6] = (durationMinutes and 0xFF).toByte()
        out[7] = ((durationMinutes ushr 8) and 0xFF).toByte()

        val unixSeconds = (System.currentTimeMillis() / 1000L).toInt()
        out[8] = (unixSeconds and 0xFF).toByte()
        out[9] = ((unixSeconds ushr 8) and 0xFF).toByte()
        out[10] = ((unixSeconds ushr 16) and 0xFF).toByte()
        out[11] = ((unixSeconds ushr 24) and 0xFF).toByte()

        val nonce = ByteArray(4)
        SecureRandom().nextBytes(nonce)
        System.arraycopy(nonce, 0, out, 12, 4)

        return out
    }

    private fun opcodeLabel(opcode: Int): String {
        return when (opcode) {
            OPCODE_LED1_SLOW -> "LED1 slow blink"
            OPCODE_LED2_FAST -> "LED2 fast blink"
            else -> "unknown"
        }
    }

    private fun selectCommand(opcode: Int) {
        selectedOpcode = opcode
        commandView.text = "Selected command: ${opcodeLabel(opcode)}"
    }

    private fun setWriteState(state: WriteState) {
        val text = when (state) {
            WriteState.READY -> "Ready - Tap to write"
            WriteState.WRITING -> "Writing..."
            WriteState.DONE -> "Done"
            WriteState.FAILED -> "No Success - Try again!"
        }
        runOnUiThread {
            writeStateView.text = text
        }
    }

    private fun signalSuccessHaptic() {
        runOnUiThread {
            window.decorView.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
        }
    }

    private fun signalFailureHaptic() {
        runOnUiThread {
            window.decorView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
        }
    }

    private fun getTagExtra(intent: Intent): Tag? {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            intent.getParcelableExtra(NfcAdapter.EXTRA_TAG, Tag::class.java)
        } else {
            @Suppress("DEPRECATION")
            intent.getParcelableExtra(NfcAdapter.EXTRA_TAG)
        }
    }

    private fun log(msg: String) {
        runOnUiThread {
            logView.append(msg)
            logView.append("\n")
        }
    }
}

private fun ByteArray.toHex(): String = joinToString("") { "%02X".format(it) }
