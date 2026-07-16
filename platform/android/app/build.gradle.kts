plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val copyYuiAssets = tasks.register<Copy>("copyYuiAssets") {
    from("../../../../app/tests/login.json") {
        into("tests")
    }
    from("../../../../app/assets") {
        into("assets")
    }
    into("src/main/assets")
}

tasks.named("preBuild") {
    dependsOn(copyYuiAssets)
}

android {
    namespace = "com.yui"
    compileSdk = 34

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
                arguments += listOf(
                    "-DYUI_REPO_ROOT=${rootProject.projectDir}/../..",
                    "-DANDROID_STL=c++_shared"
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
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.core:core-ktx:1.13.1")
}
