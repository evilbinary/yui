import UIKit
import MetalKit

class YuiViewController: UIViewController {
    private var metalView: MTKView!
    private var displayLink: CADisplayLink?
    private var running = false
    private var dataDir: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("yui", isDirectory: true)
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black

        metalView = MTKView(frame: view.bounds)
        metalView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        metalView.device = MTLCreateSystemDefaultDevice()
        metalView.isPaused = true
        metalView.enableSetNeedsDisplay = false
        metalView.framebufferOnly = true
        metalView.clearColor = MTLClearColorMake(0.12, 0.12, 0.12, 1.0)
        view.addSubview(metalView)

        let tap = UITapGestureRecognizer(target: self, action: #selector(handleTap(_:)))
        metalView.addGestureRecognizer(tap)

        NotificationCenter.default.addObserver(
            self,
            selector: #selector(appWillResignActive),
            name: UIApplication.willResignActiveNotification,
            object: nil
        )
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(appDidBecomeActive),
            name: UIApplication.didBecomeActiveNotification,
            object: nil
        )
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        metalView.drawableSize = CGSize(
            width: metalView.bounds.width * metalView.contentScaleFactor,
            height: metalView.bounds.height * metalView.contentScaleFactor
        )
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        startYuiIfNeeded()
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        YuiBridge.setAppFocused(false)
        pause()
    }

    @objc private func appWillResignActive() {
        YuiBridge.setAppFocused(false)
    }

    @objc private func appDidBecomeActive() {
        guard running else { return }
        YuiBridge.setAppFocused(true)
    }

    func pause() {
        running = false
        displayLink?.invalidate()
        displayLink = nil
    }

    func resume() {
        guard metalView.layer is CAMetalLayer else { return }
        if !running {
            startYuiIfNeeded()
        }
    }

    private func startYuiIfNeeded() {
        guard let metalLayer = metalView.layer as? CAMetalLayer else { return }
        if running { return }

        prepareAssets()
        let jsonPath = dataDir.appendingPathComponent("app/watch-os/app.json").path
        let assetsPath = dataDir.appendingPathComponent("app/assets").path
        let density = Float(UIScreen.main.scale)

        YuiBridge.init(
            layer: metalLayer,
            jsonPath: jsonPath,
            assetsPath: assetsPath,
            dataRoot: dataDir.path,
            density: density
        )

        let size = metalView.drawableSize
        YuiBridge.resize(withWidth: Int32(size.width), height: Int32(size.height))

        running = true
        displayLink = CADisplayLink(target: self, selector: #selector(onFrame))
        displayLink?.add(to: .main, forMode: .common)
        YuiBridge.setAppFocused(true)
    }

    @objc private func onFrame() {
        guard running else { return }
        YuiBridge.tick()
    }

    @objc private func handleTap(_ recognizer: UITapGestureRecognizer) {
        guard running else { return }
        let point = recognizer.location(in: metalView)
        let scale = metalView.contentScaleFactor
        let x = Float(point.x * scale)
        let y = Float(point.y * scale)
        YuiBridge.onTouch(withPointerId: 0, x: x, y: y, phase: 0)
        YuiBridge.onTouch(withPointerId: 0, x: x, y: y, phase: 2)
    }

    private func prepareAssets() {
        let fm = FileManager.default
        try? fm.createDirectory(at: dataDir, withIntermediateDirectories: true)
        copyBundleResources(at: "", into: dataDir)
    }

    private func copyBundleResources(at subpath: String, into destRoot: URL) {
        guard let bundleRoot = Bundle.main.resourcePath else { return }
        let sourceDir = subpath.isEmpty ? bundleRoot : (bundleRoot as NSString).appendingPathComponent(subpath)
        guard let items = try? FileManager.default.contentsOfDirectory(atPath: sourceDir) else { return }

        for name in items {
            let relative = subpath.isEmpty ? name : "\(subpath)/\(name)"
            let sourcePath = (bundleRoot as NSString).appendingPathComponent(relative)
            var isDir: ObjCBool = false
            guard FileManager.default.fileExists(atPath: sourcePath, isDirectory: &isDir) else { continue }
            if isDir.boolValue {
                copyBundleResources(at: relative, into: destRoot)
            } else {
                let dest = destRoot.appendingPathComponent(relative)
                try? FileManager.default.createDirectory(
                    at: dest.deletingLastPathComponent(),
                    withIntermediateDirectories: true
                )
                if !FileManager.default.fileExists(atPath: dest.path) {
                    try? FileManager.default.copyItem(atPath: sourcePath, toPath: dest.path)
                }
            }
        }
    }

    deinit {
        NotificationCenter.default.removeObserver(self)
        pause()
        YuiBridge.shutdown()
    }
}
