// Draws Paper's app icon: a warm gradient squircle (the launcher orb's
// palette) with a folded sheet of paper. Writes an .iconset for iconutil.
// Usage: swift make-icon.swift <out.iconset>
import AppKit

let out = URL(fileURLWithPath: CommandLine.arguments[1])
try? FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)

func draw(size: CGFloat) -> NSImage {
    NSImage(size: NSSize(width: size, height: size), flipped: false) { _ in
        let s = size / 1024
        // macOS icon grid: ~824pt body inside a 1024 canvas.
        let body = NSRect(x: 100 * s, y: 100 * s, width: 824 * s, height: 824 * s)
        let squircle = NSBezierPath(roundedRect: body, xRadius: 185 * s, yRadius: 185 * s)

        NSGraphicsContext.saveGraphicsState()
        let shadow = NSShadow()
        shadow.shadowColor = NSColor.black.withAlphaComponent(0.25)
        shadow.shadowOffset = NSSize(width: 0, height: -10 * s)
        shadow.shadowBlurRadius = 24 * s
        shadow.set()
        NSColor.black.setFill()
        squircle.fill()
        NSGraphicsContext.restoreGraphicsState()

        NSGradient(colors: [
            NSColor(calibratedHue: 0.08, saturation: 0.78, brightness: 1.0, alpha: 1),
            NSColor(calibratedHue: 0.96, saturation: 0.66, brightness: 0.96, alpha: 1),
            NSColor(calibratedHue: 0.76, saturation: 0.62, brightness: 0.86, alpha: 1),
        ])!.draw(in: squircle, angle: -55)

        // Glass sheen across the top.
        NSGraphicsContext.saveGraphicsState()
        squircle.addClip()
        NSGradient(colors: [NSColor.white.withAlphaComponent(0.35), NSColor.white.withAlphaComponent(0)])!
            .draw(in: NSRect(x: body.minX, y: body.midY, width: body.width, height: body.height / 2), angle: -90)
        NSGraphicsContext.restoreGraphicsState()

        // A sheet of paper with its top-right corner folded.
        let sheet = NSRect(x: 322 * s, y: 262 * s, width: 380 * s, height: 500 * s)
        let fold: CGFloat = 120 * s
        let page = NSBezierPath()
        page.move(to: NSPoint(x: sheet.minX, y: sheet.minY))
        page.line(to: NSPoint(x: sheet.maxX, y: sheet.minY))
        page.line(to: NSPoint(x: sheet.maxX, y: sheet.maxY - fold))
        page.line(to: NSPoint(x: sheet.maxX - fold, y: sheet.maxY))
        page.line(to: NSPoint(x: sheet.minX, y: sheet.maxY))
        page.close()
        NSGraphicsContext.saveGraphicsState()
        let pageShadow = NSShadow()
        pageShadow.shadowColor = NSColor.black.withAlphaComponent(0.22)
        pageShadow.shadowOffset = NSSize(width: 0, height: -14 * s)
        pageShadow.shadowBlurRadius = 30 * s
        pageShadow.set()
        NSColor.white.withAlphaComponent(0.96).setFill()
        page.fill()
        NSGraphicsContext.restoreGraphicsState()

        let corner = NSBezierPath()
        corner.move(to: NSPoint(x: sheet.maxX - fold, y: sheet.maxY))
        corner.line(to: NSPoint(x: sheet.maxX - fold, y: sheet.maxY - fold))
        corner.line(to: NSPoint(x: sheet.maxX, y: sheet.maxY - fold))
        corner.close()
        NSColor(calibratedHue: 0.95, saturation: 0.25, brightness: 0.92, alpha: 1).setFill()
        corner.fill()

        // Three soft lines of "text".
        NSColor(calibratedHue: 0.93, saturation: 0.35, brightness: 0.85, alpha: 0.55).setFill()
        for (i, width) in [230.0, 260.0, 170.0].enumerated() {
            let y = sheet.minY + (300 - Double(i) * 78) * s
            NSBezierPath(roundedRect: NSRect(x: sheet.minX + 60 * s, y: y, width: width * s, height: 30 * s),
                         xRadius: 15 * s, yRadius: 15 * s).fill()
        }
        return true
    }
}

for base in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let px = CGFloat(base * scale)
        let image = draw(size: px)
        guard let tiff = image.tiffRepresentation, let rep = NSBitmapImageRep(data: tiff),
              let png = rep.representation(using: .png, properties: [:]) else { continue }
        let name = scale == 1 ? "icon_\(base)x\(base).png" : "icon_\(base)x\(base)@2x.png"
        try png.write(to: out.appendingPathComponent(name))
    }
}
