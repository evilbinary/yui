import UIKit

@main
class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?

    func application(
        _ application: UIApplication,
        didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
    ) -> Bool {
        let window = UIWindow(frame: UIScreen.main.bounds)
        window.rootViewController = YuiViewController()
        window.makeKeyAndVisible()
        self.window = window
        return true
    }

    func applicationWillResignActive(_ application: UIApplication) {
        if let controller = window?.rootViewController as? YuiViewController {
            controller.pause()
        }
    }

    func applicationDidBecomeActive(_ application: UIApplication) {
        if let controller = window?.rootViewController as? YuiViewController {
            controller.resume()
        }
    }
}
