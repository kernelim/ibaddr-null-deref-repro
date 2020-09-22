A reproducer for the following issue in the IB stack:

```
[165371.631784] Workqueue: ib_addr process_one_req [ib_core]
[165371.637268] RIP: 0010:0x0
[165371.640066] Code: Bad RIP value.
[165371.643468] RSP: 0018:ffffb484cfd87e60 EFLAGS: 00010297
[165371.648870] RAX: 0000000000000000 RBX: ffff94ef2e027130 RCX: ffff94eee8271800
[165371.656196] RDX: ffff94eee8271920 RSI: ffff94ef2e027010 RDI: 00000000ffffff92
[165371.663518] RBP: ffffb484cfd87e80 R08: 00726464615f6269 R09: 8080808080808080
[165371.670839] R10: ffffb484cfd87c68 R11: fefefefefefefeff R12: ffff94ef2e027000
[165371.678162] R13: ffff94ef2e027010 R14: ffff94ef2e027130 R15: 0ffff951f2c624a0
[165371.685485] FS:  0000000000000000(0000) GS:ffff94ef40e80000(0000) knlGS:0000000000000000
[165371.693762] CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
[165371.699681] CR2: ffffffffffffffd6 CR3: 0000005eca20a002 CR4: 00000000007606e0
[165371.707001] DR0: 0000000000000000 DR1: 0000000000000000 DR2: 0000000000000000
[165371.714325] DR3: 0000000000000000 DR6: 00000000fffe0ff0 DR7: 0000000000000400
[165371.721647] PKRU: 55555554
[165371.724526] Call Trace:
[165371.727170]  process_one_req+0x39/0x150 [ib_core]
[165371.732051]  process_one_work+0x20f/0x400
[165371.736242]  worker_thread+0x34/0x410
[165371.740082]  kthread+0x121/0x140
[165371.743484]  ? process_one_work+0x400/0x400
[165371.747844]  ? kthread_park+0x90/0x90
[165371.751681]  ret_from_fork+0x1f/0x40

```