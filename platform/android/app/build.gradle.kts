plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val yuiRepoRoot = rootProject.projectDir.resolve("../..").normalize()
val yuiAssetsDir = layout.buildDirectory.dir("generated/yuiAssets")

val copyYuiAssets = tasks.register<Copy>("copyYuiAssets") {
    from(yuiRepoRoot.resolve("app/assets")) {
        into("app/assets")
    }
    from(yuiRepoRoot.resolve("app/watch-os")) {
        into("app/watch-os")
    }
    from(yuiRepoRoot.resolve("app/lib")) {
        into("app/lib")
    }
    from(yuiRepoRoot.resolve("app/tests/login.json")) {
        into("app/tests")
    }
    into(yuiAssetsDir)
}

android {
    namespace = "com.yui"
    compileSdk = 34
    ndkVersion = "27.3.13750724"

    sourceSets {
        getByName("main") {
            assets.srcDir(yuiAssetsDir)
        }
    }

    defaultConfig {
        applicationId = "com.yui"
        minSdk = 21
        targetSdk = 34
        versionCode = 1
        versionName = "0.1.0"
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17", "-DHAS_JS_MODULE")
                val yuiRepoRootPath = yuiRepoRoot.toString().replace('\\', '/')
                arguments += listOf(
                    "-DYUI_REPO_ROOT=$yuiRepoRootPath",
                    "-DANDROID_STL=c++_shared",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"
                )
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    externalNativeBuild {
        cmake {
            version = "3.22.1"
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

tasks.named("preBuild") {
    dependsOn(copyYuiAssets)
}

tasks.configureEach {
    if (name.startsWith("merge") && name.endsWith("Assets")) {
        dependsOn(copyYuiAssets)
    }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.core:core-ktx:1.13.1")
}
