package io.github.cylear.hoshimi.localify

import android.app.Activity
import android.content.Intent
import android.widget.Toast
import androidx.core.content.FileProvider
import io.github.cylear.hoshimi.localify.mainUtils.json
import io.github.cylear.hoshimi.localify.models.IdolyprideConfig
import io.github.cylear.hoshimi.localify.models.ProgramConfig
import io.github.cylear.hoshimi.localify.models.ProgramConfigSerializer
import kotlinx.serialization.SerializationException
import java.io.File


interface IHasConfigItems {
    var config: IdolyprideConfig
    var programConfig: ProgramConfig

    fun saveConfig() {}  // do nothing
}

interface IConfigurableActivity<T : Activity> : IHasConfigItems


fun <T> T.getConfigContent(): String where T : Activity {
    val configFile = File(filesDir, "ipr-config.json")
    return if (configFile.exists()) {
        configFile.readText()
    } else {
        Toast.makeText(this, "Initializing...", Toast.LENGTH_SHORT).show()
        configFile.writeText("{}")
        "{}"
    }
}

fun <T> T.getProgramConfigContent(
    excludes: List<String> = emptyList(),
    origProgramConfig: ProgramConfig? = null
): String where T : Activity {
    val configFile = File(filesDir, "localify-config.json")
    if (excludes.isEmpty()) {
        return if (configFile.exists()) {
            configFile.readText()
        } else {
            "{}"
        }
    } else {
        return if (origProgramConfig == null) {
            if (configFile.exists()) {
                val parsedConfig = json.decodeFromString<ProgramConfig>(configFile.readText())
                json.encodeToString(ProgramConfigSerializer(excludes), parsedConfig)
            } else {
                "{}"
            }
        } else {
            json.encodeToString(ProgramConfigSerializer(excludes), origProgramConfig)
        }
    }
}

fun <T> T.loadConfig() where T : Activity, T : IHasConfigItems {
    val configStr = getConfigContent()
    config = try {
        json.decodeFromString<IdolyprideConfig>(configStr)
    } catch (e: SerializationException) {
        Toast.makeText(this, "?�置?�件异常: $e", Toast.LENGTH_SHORT).show()
        IdolyprideConfig()
    }
    saveConfig()

    val programConfigStr = getProgramConfigContent()
    programConfig = try {
        json.decodeFromString<ProgramConfig>(programConfigStr)
    } catch (e: SerializationException) {
        ProgramConfig()
    }
}

fun <T> T.onClickStartGame() where T : Activity, T : IHasConfigItems {
    val intent = Intent().apply {
        setClassName(
            "game.qualiarts.idolypride",
            "com.google.firebase.MessagingUnityPlayerActivity"
        )
        putExtra("iprData", getConfigContent())
        putExtra(
            "localData",
            getProgramConfigContent(listOf("transRemoteZipUrl", "p"), programConfig)
        )
        flags = Intent.FLAG_ACTIVITY_NEW_TASK
    }

    val updateFile = File(filesDir, "update_trans.zip")
    if (updateFile.exists()) {
        val dirUri = FileProvider.getUriForFile(
            this,
            "io.github.cylear.hoshimi.localify.fileprovider",
            File(updateFile.absolutePath)
        )
        intent.setDataAndType(dirUri, "resource/file")
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
    }

    startActivity(intent)
}
