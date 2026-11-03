backup: 2026-10-25T23:53:53Z
public HEAD: 58b0788d6972f66a058a8b8c35e8035972156e0e
ci HEAD: 47afeb281e91d8b89e6b8c9a66481b8590d29007
NativeEmmc HEAD: 4980f1070b0c822dedb9eb350df710a57df28590

Latest panic-report image: images/merlinx-uefi-emmc-panicfix.img (7dafb342a708a21b15718a0616b7b091f7f741c6926753b9b400a989bbf29f63)
Latest kernel: kernel/kernel-panicfix.macho (9877583abb0b2729cd2f1846a388491dc6dabf2025d4c9c4e5f2e235611e4857)
Latest payload: payload/payload-panicfix.img (7d4702da9f70812ecc4ab55b5ef25c96b7de63915213b57c85eb973bb40cad83)

The initproc panic message now starts with [owner ...]; the owner report is prefixed and is also printed before the panic.

Restore bundles:
  git clone git-bundles/public-merlinx-work.bundle UniXNU-merlinx
  git clone git-bundles/private-merlinx-ci.bundle UniXNU-merlinx-ci
  git clone git-bundles/NativeEmmc-main.bundle NativeEmmc
