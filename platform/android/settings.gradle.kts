import java.io.File
import java.util.Properties

fun resolveAndroidSdkDir(): String? = sequenceOf(
    System.getenv("ANDROID_HOME"),
    System.getenv("ANDROID_SDK_ROOT"),
).firstOrNull { path -> !path.isNullOrBlank() && File(path).isDirectory }

fun escapeLocalPropPath(path: String): String = path.replace("\\", "\\\\")

fun ensureLocalProperties() {
    val localPropsFile = File(settingsDir, "local.properties")
    val props = Properties()
    if (localPropsFile.exists()) {
        localPropsFile.inputStream().use { props.load(it) }
    }

    var changed = false

    val configuredSdk = props.getProperty("sdk.dir")?.replace("\\\\", "\\")
    val sdkValid = configuredSdk != null && File(configuredSdk).isDirectory
    val sdkFromEnv = resolveAndroidSdkDir()
    if (!sdkValid && sdkFromEnv != null) {
        props.setProperty("sdk.dir", escapeLocalPropPath(sdkFromEnv))
        changed = true
    }

    // cmake.dir overrides SDK cmake; remove stale paths (use sdk.dir + cmake version instead).
    val cmakeDir = props.getProperty("cmake.dir")?.replace("\\\\", "\\")
    if (cmakeDir != null) {
        val hasCmake = File(cmakeDir, "bin/cmake.exe").exists() || File(cmakeDir, "bin/cmake").exists()
        if (!hasCmake) {
            props.remove("cmake.dir")
            changed = true
        }
    }

    if (changed) {
        localPropsFile.outputStream().use {
            props.store(it, "Auto-synced from ANDROID_HOME / ANDROID_SDK_ROOT")
        }
    }
}

ensureLocalProperties()

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    // PREFER_SETTINGS: compatible with ~/.gradle/init.gradle (allprojects.repositories).
    repositoriesMode.set(RepositoriesMode.PREFER_SETTINGS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "YuiAndroid"
include(":app")
