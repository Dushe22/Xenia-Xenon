project_root = "../../../.."
include(project_root.."/tools/build")

group("src")
project("xenia-hid-portal")
  uuid("ddc114da-a279-4868-8b20-53108599bd78")
  kind("StaticLib")
  language("C++")
  files({
    "portal.h",
    "portal.cc",
  })

  filter({"platforms:Windows"})
    links({
      "libusb",
    })
    files({
      "hardware_portal.h",
      "hardware_portal.cc",
    })
