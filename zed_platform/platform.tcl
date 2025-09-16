# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct C:\Users\priya\OneDrive\Desktop\AES_ENCRYPTION\SD\zed_platform\platform.tcl
# 
# OR launch xsct and run below command.
# source C:\Users\priya\OneDrive\Desktop\AES_ENCRYPTION\SD\zed_platform\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {zed_platform}\
-hw {C:\Users\priya\aes_1\design_2_wrapper.xsa}\
-proc {ps7_cortexa9_0} -os {standalone} -fsbl-target {psu_cortexa53_0} -out {C:/Users/priya/OneDrive/Desktop/AES_ENCRYPTION/SD}

platform write
platform generate -domains 
platform active {zed_platform}
domain active {zynq_fsbl}
bsp reload
bsp removelib -name xilffs
bsp setlib -name xilffs -ver 4.4
bsp write
bsp reload
catch {bsp regenerate}
domain active {standalone_domain}
bsp reload
bsp setlib -name xilffs -ver 4.4
bsp write
bsp reload
catch {bsp regenerate}
platform generate
platform clean
platform generate
platform clean
platform generate
platform clean
platform generate
