# stcping
A tcping tool for linux based on select



Usage

  stcping [options] IP/Domain Port

Options:

  -c <count>	stop after <count> replies

  -4		use IPv4

  -6		use IPv6

  -h		print help and exit


Result:

\# ./stcping www.google.com 443

response from 172.217.24.100:443 1.49 ms
	
response from 172.217.24.100:443 1.28 ms
	
response from 172.217.24.100:443 1.51 ms
	
response from 172.217.24.100:443 1.42 ms
	
^C
	
Ping statistics for 172.217.24.100:443
	
	4 sent.
	
	4 successful, 0 failed.
	
	0.00% fail.
	
	Minimum = 1.28ms, Maximum = 1.51ms, Average = 1.43ms

