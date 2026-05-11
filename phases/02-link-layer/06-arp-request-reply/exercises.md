# Exercises for 06. ARP request and reply

## ★ Watch the wire

While `arp_responder` is running:

```bash
sudo tcpdump -i tap0 -n -e arp
```

In another shell, `arping -c 3 -I tap0 10.0.0.42`. Identify the request and reply packets in the tcpdump output. Note the broadcast destination on the request and the unicast destination on the reply.

## ★ Inspect the host's ARP cache

```bash
ip neigh show dev tap0
```

What state is the entry in (REACHABLE, STALE, FAILED, etc.)? After 60 seconds of no traffic, run `ip neigh show` again. What changed?

## ★★ Gratuitous ARP

A *gratuitous* ARP is an unsolicited reply (or a request to your own IP) sent to the broadcast address. Hosts that see it update their cache without having asked.

Modify `arp.c` so that on startup, before the read loop, it sends one gratuitous ARP. Verify with `tcpdump` that the host receives it and `ip neigh show` shows our entry without ever calling `arping`.

## ★★ Reply opcode validation

Right now we silently drop everything that isn't an ARP request for our IP. What about ARP **replies** that we receive? Add a counter that increments every time we see an unsolicited reply, and print it on SIGINT (`Ctrl-C`). What does it tell you about the host's behaviour?

## ★★★ ARP cache with TTL

Implement a small ARP cache (`struct arp_entry { uint8_t mac[6]; uint32_t ip; time_t last_seen; }`) sized at 256 entries. Every reply we send adds the *requester's* IP→MAC mapping. Evict entries older than 60 seconds.

Why 60 seconds is a poor default for high-churn environments: in Kubernetes, a pod that dies and restarts may reuse an IP within seconds with a different MAC. A 60-second cache holds the wrong answer. Production CNI plugins clear ARP entries on pod teardown explicitly. Document this in your code.

## ★★★ Defeat ARP poisoning detection

Some hosts run `arpwatch`-style daemons that alert when a known IP→MAC mapping changes. Read the `arpwatch` source. What pattern would you have to follow to update an IP-to-MAC mapping without triggering the alert? (Hint: there isn't a clean way. That's the point.)
