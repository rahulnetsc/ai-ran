#!/usr/bin/env python3
"""
NR Simulation Telemetry Monitor

Listens for real-time telemetry from ns-3 NR simulation via UDP.
Displays UE positions, attachments, traffic stats, and handovers.

Usage:
    python3 monitor_telemetry.py [port]

Example:
    python3 monitor_telemetry.py 5555
"""

import socket
import json
import sys
import time
from datetime import datetime
from collections import defaultdict

class TelemetryMonitor:
    def __init__(self, port=5555, verbose=False):
        self.port = port
        self.verbose = verbose
        self.stats = {
            'packets_received': 0,
            'packets_failed': 0,
            'last_update': None,
            'start_time': time.time()
        }
        self.ue_history = defaultdict(list)
        
    def run(self):
        """Main monitoring loop"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(('0.0.0.0', self.port))
        
        print("="*70)
        print(f"NR Simulation Telemetry Monitor")
        print(f"Listening on UDP port {self.port}")
        print(f"Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("="*70)
        print("\nWaiting for simulation updates...")
        print("(Press Ctrl+C to stop)\n")
        
        try:
            while True:
                data, addr = sock.recvfrom(65536)
                self.process_packet(data)
                
        except KeyboardInterrupt:
            print("\n\nStopping monitor...")
            self.print_final_stats()
        except Exception as e:
            print(f"\nError: {e}")
            self.stats['packets_failed'] += 1
    
    def process_packet(self, data):
        """Process received telemetry packet"""
        try:
            state = json.loads(data.decode('utf-8'))
            self.stats['packets_received'] += 1
            self.stats['last_update'] = datetime.now()
            
            self.display_state(state)
            
        except json.JSONDecodeError as e:
            print(f"JSON parse error: {e}")
            self.stats['packets_failed'] += 1
        except Exception as e:
            print(f"Processing error: {e}")
            self.stats['packets_failed'] += 1
    
    def display_state(self, state):
        """Display simulation state"""
        sim_time = state['timestamp']['simulation_time']
        status = state['simulation'].get('status', 'unknown')
        progress = state['simulation'].get('progress_percent', 0)
        
        # Header
        print("\n" + "="*70)
        print(f"Time: {sim_time:.3f}s | Status: {status} | Progress: {progress:.1f}%")
        print("="*70)
        
        # Network topology
        ue_count = state['config']['ue_count']
        gnb_count = state['config']['gnb_count']
        print(f"\nNetwork: {gnb_count} gNBs, {ue_count} UEs")
        
        # UE details
        print("\nUE Status:")
        print(f"{'ID':<4} {'IMSI':<6} {'Position':<20} {'Velocity':<15} {'Cell':<6} {'Distance':<10}")
        print("-"*70)
        
        for ue in state['topology']['ues']:
            ue_id = ue['id']
            imsi = ue['imsi']
            
            # Position
            if 'position' in ue:
                pos = ue['position']
                pos_str = f"({pos['x']:.1f}, {pos['y']:.1f})"
            else:
                pos_str = "N/A"
            
            # Velocity
            if 'velocity' in ue:
                vel = ue['velocity']
                speed = ue.get('speed', 0)
                vel_str = f"{speed:.1f} m/s"
            else:
                vel_str = "N/A"
            
            # Network
            if 'network' in ue:
                cell_id = ue['network']['cell_id']
                distance = ue['network'].get('distance_to_gnb', 0)
                dist_str = f"{distance:.1f} m"
            else:
                cell_id = "N/A"
                dist_str = "N/A"
            
            print(f"{ue_id:<4} {imsi:<6} {pos_str:<20} {vel_str:<15} {cell_id:<6} {dist_str:<10}")
        
        # Traffic summary
        if 'traffic_summary' in state:
            traffic = state['traffic_summary']
            print(f"\nTraffic Summary:")
            print(f"  Total DL: {traffic['total_dl_throughput_mbps']:.2f} Mbps")
            print(f"  Total UL: {traffic['total_ul_throughput_mbps']:.2f} Mbps")
            print(f"  Avg Loss: {traffic['avg_packet_loss_percent']:.1f}%")
        
        # Per-UE traffic
        if self.verbose and 'topology' in state:
            has_traffic = any('traffic' in ue for ue in state['topology']['ues'])
            if has_traffic:
                print(f"\nPer-UE Traffic:")
                print(f"{'UE':<4} {'DL Mbps':<10} {'DL Loss%':<10} {'UL Mbps':<10} {'UL Loss%':<10}")
                print("-"*50)
                
                for ue in state['topology']['ues']:
                    if 'traffic' in ue:
                        ue_id = ue['id']
                        dl_mbps = ue['traffic']['dl']['throughput_mbps']
                        dl_loss = ue['traffic']['dl']['loss_percent']
                        ul_mbps = ue['traffic']['ul']['throughput_mbps']
                        ul_loss = ue['traffic']['ul']['loss_percent']
                        
                        print(f"{ue_id:<4} {dl_mbps:<10.2f} {dl_loss:<10.1f} {ul_mbps:<10.2f} {ul_loss:<10.1f}")
        
        # Handovers
        if 'handovers' in state:
            total_ho = state['handovers']['total_count']
            recent_ho = state['handovers'].get('recent_events', [])
            
            if total_ho > 0:
                print(f"\nHandovers: {total_ho} total")
                
                if recent_ho and self.verbose:
                    print("\nRecent Handovers:")
                    for ho in recent_ho[-5:]:  # Last 5
                        t = ho['timestamp']
                        ue = ho['ue_id']
                        src = ho['source_cell_id']
                        tgt = ho['target_cell_id']
                        ok = '✓' if ho['success'] else '✗'
                        print(f"  t={t:.3f}s: UE {ue} | Cell {src} → {tgt} {ok}")
        
        # Events
        if self.verbose and 'events' in state:
            recent_events = state['events'].get('recent', [])
            if recent_events:
                print("\nRecent Events:")
                for evt in recent_events[-3:]:  # Last 3
                    t = evt['timestamp']
                    typ = evt['type']
                    desc = evt['description']
                    print(f"  {t:.3f}s [{typ}]: {desc}")
        
        # Alerts
        self.check_alerts(state)
        
        # Update rate
        elapsed = time.time() - self.stats['start_time']
        rate = self.stats['packets_received'] / elapsed if elapsed > 0 else 0
        print(f"\nUpdates: {self.stats['packets_received']} ({rate:.1f}/s)")
    
    def check_alerts(self, state):
        """Check for issues and display alerts"""
        alerts = []
        
        # High packet loss
        if 'traffic_summary' in state:
            loss = state['traffic_summary']['avg_packet_loss_percent']
            if loss > 50:
                alerts.append(f"⚠️  HIGH PACKET LOSS: {loss:.1f}%")
            elif loss > 30:
                alerts.append(f"⚠️  Elevated packet loss: {loss:.1f}%")
        
        # UE issues
        if 'topology' in state:
            for ue in state['topology']['ues']:
                ue_id = ue['id']
                
                # Stuck UE
                if 'velocity' in ue:
                    speed = ue.get('speed', 0)
                    mobility = ue.get('mobility_model', '')
                    if speed == 0 and mobility == 'waypoint':
                        alerts.append(f"⚠️  UE {ue_id} not moving (waypoint)")
                
                # High loss
                if 'traffic' in ue:
                    dl_loss = ue['traffic']['dl']['loss_percent']
                    if dl_loss > 80:
                        alerts.append(f"⚠️  UE {ue_id} severe DL loss: {dl_loss:.1f}%")
        
        # Cell overload
        if 'topology' in state and 'gnbs' in state['topology']:
            for gnb in state['topology']['gnbs']:
                ue_count = gnb['attached_ues']['count']
                cell_id = gnb['cell_id']
                if ue_count > 8:
                    alerts.append(f"⚠️  Cell {cell_id} overloaded: {ue_count} UEs")
                elif ue_count == 0:
                    alerts.append(f"⚠️  Cell {cell_id} has no UEs")
        
        if alerts:
            print("\n🚨 ALERTS:")
            for alert in alerts:
                print(f"  {alert}")
    
    def print_final_stats(self):
        """Print final statistics"""
        print("\n" + "="*70)
        print("Final Statistics")
        print("="*70)
        
        print(f"Packets received: {self.stats['packets_received']}")
        print(f"Packets failed: {self.stats['packets_failed']}")
        
        if self.stats['packets_received'] > 0:
            success_rate = 100 * self.stats['packets_received'] / \
                          (self.stats['packets_received'] + self.stats['packets_failed'])
            print(f"Success rate: {success_rate:.1f}%")
        
        elapsed = time.time() - self.stats['start_time']
        print(f"Duration: {elapsed:.1f} seconds")
        
        if elapsed > 0:
            rate = self.stats['packets_received'] / elapsed
            print(f"Average rate: {rate:.2f} updates/sec")
        
        if self.stats['last_update']:
            print(f"Last update: {self.stats['last_update'].strftime('%H:%M:%S')}")
        
        print("="*70)

def main():
    """Main entry point"""
    port = 5555
    verbose = False
    
    # Parse command line arguments
    for arg in sys.argv[1:]:
        if arg.startswith('-'):
            if arg in ['-v', '--verbose']:
                verbose = True
            elif arg in ['-h', '--help']:
                print(__doc__)
                sys.exit(0)
        else:
            try:
                port = int(arg)
            except ValueError:
                print(f"Invalid port: {arg}")
                sys.exit(1)
    
    # Create and run monitor
    monitor = TelemetryMonitor(port=port, verbose=verbose)
    monitor.run()

if __name__ == "__main__":
    main()
