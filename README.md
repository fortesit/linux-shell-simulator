 Linux shell simulator

 Version: 1.0
 GitHub repository: https://github.com/fortesit/linux-shell-simulator
 Author: Sit King Lok
 Last modified: 2014-09-26 19:48
 
 Description:
 A simplified Linux shell with the following features
 1. Accept most of the commands with arguments (e.g. cd, ls -a, exit)
 2. I/O redirection (i.e. >, >>, <, <<)
 3. Pipes (i.e. |)
 4. Signal handling (e.g. Ctrl+Z, Ctrl+C)
 5. Job control (i.e. jobs, fg)

 Usage:
 gcc shell.c -o shell
 ./shell

 Platform:
 Unix(Mac/Linux)

 Note:
 All arguments should have a white-space in between.
 
 Example:
 airman2:~ Sit$ ./shell
 [3150 shell:/Users/Sit]$ mkdir test
 [3150 shell:/Users/Sit]$ cd test
 [3150 shell:/Users/Sit/test]$ touch new_file
 [3150 shell:/Users/Sit/test]$ cat | cat >> new_file
 Hello
 ^Z
 [3150 shell:/Users/Sit/test]$
 
 [3150 shell:/Users/Sit/test]$ ping -c 2 8.8.8.8 | cat >> new_file
 [3150 shell:/Users/Sit/test]$ jobs
 [1]: cat | cat >> new_file
 [3150 shell:/Users/Sit/test]$ fg 1
 Job wake up: cat | cat >> new_file
 World
 ^C[3150 shell:/Users/Sit/test]$ cat new_file
 Hello
 PING 8.8.8.8 (8.8.8.8): 56 data bytes
 64 bytes from 8.8.8.8: icmp_seq=0 ttl=49 time=32.598 ms
 64 bytes from 8.8.8.8: icmp_seq=1 ttl=49 time=31.467 ms
 
 --- 8.8.8.8 ping statistics ---
 2 packets transmitted, 2 packets received, 0.0% packet loss
 round-trip min/avg/max/stddev = 31.467/32.032/32.598/0.566 ms
 
 World
 [3150 shell:/Users/Sit/test]$ exit
 airman2:~ Sit$
