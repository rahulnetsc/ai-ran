#!/usr/bin/env python3
"""
Advanced NR Simulation Telemetry Dashboard
Includes: Live Mapping with Connection Lines, Per-UE Throughput, and Raw Data Inspector.
"""

import socket
import json
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# --- Visual Palette ---
C = {
    "bg": "#f4f6f9",
    "card": "#ffffff",
    "text": "#1e2430",
    "text_dim": "#6b7280",
    "accent_blue": "#3b82f6",
    "accent_green": "#16a34a",
    "accent_red": "#dc2626",
    "border": "#dce1e7",
    "header_bg": "#2c3e50",
    "header_fg": "#ffffff"
}

class TelemetryReceiver(threading.Thread):
    def __init__(self, port):
        super().__init__(daemon=True)
        self.port = port
        self.latest_data = None
        self.raw_message = ""
        self.lock = threading.Lock()

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("0.0.0.0", self.port))
        while True:
            try:
                data, _ = sock.recvfrom(65536)
                raw_str = data.decode("utf-8")
                decoded = json.loads(raw_str)
                with self.lock:
                    self.latest_data = decoded
                    self.raw_message = raw_str
            except Exception as e:
                print(f"Receiver Error: {e}")

class TelemetryGUI(tk.Tk):
    def __init__(self, port):
        super().__init__()
        self.title("Advanced NR Telemetry Dashboard")
        self.geometry("1500x950")
        self.configure(bg=C["bg"])
        
        self.paused = tk.BooleanVar(value=False)
        self.receiver = TelemetryReceiver(port)
        self.receiver.start()
        
        self._setup_styles()
        self._build_ui()
        self._refresh()

    def _setup_styles(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("Treeview", background=C["card"], fieldbackground=C["card"], foreground=C["text"], font=("Segoe UI", 9))
        style.configure("Treeview.Heading", font=("Segoe UI", 9, "bold"))
        style.configure("TProgressbar", thickness=15)

    def _build_ui(self):
        # --- Header ---
        header = tk.Frame(self, bg=C["header_bg"], height=70)
        header.pack(fill="x")
        tk.Label(header, text="NR 5G LIVE MONITOR", bg=C["header_bg"], fg=C["header_fg"], font=("Helvetica", 16, "bold")).pack(side="left", padx=20)
        
        self.lbl_time = tk.Label(header, text="Sim Time: 0.000s", bg=C["header_bg"], fg=C["header_fg"], font=("Consolas", 14))
        self.lbl_time.pack(side="right", padx=20)

        self.progress = ttk.Progressbar(self, orient="horizontal", mode="determinate")
        self.progress.pack(fill="x")

        # --- Top Row: Global Stats ---
        stats_bar = tk.Frame(self, bg=C["bg"])
        stats_bar.pack(fill="x", padx=10, pady=10)
        
        self.card_dl = self._create_stat_card(stats_bar, "Total DL Throughput", "0.00 Mbps")
        self.card_ul = self._create_stat_card(stats_bar, "Total UL Throughput", "0.00 Mbps")
        self.card_loss = self._create_stat_card(stats_bar, "Avg Network Loss", "0.0%")
        self.card_scheduler = self._create_stat_card(stats_bar, "Scheduler", "—")
        self.card_config = self._create_stat_card(stats_bar, "Network Config", "3 gNBs | 10 UEs")

        # --- Main Body ---
        main_frame = tk.Frame(self, bg=C["bg"])
        main_frame.pack(fill="both", expand=True, padx=10)

        # Left Column: Map
        left_col = tk.Frame(main_frame, bg=C["bg"])
        left_col.pack(side="left", fill="both", expand=True)
        
        plot_frame = tk.LabelFrame(left_col, text="Live Topology & Connections", bg=C["card"])
        plot_frame.pack(fill="both", expand=True, padx=5, pady=5)
        
        self.fig, self.ax = plt.subplots(figsize=(6, 6), dpi=100)
        self.canvas = FigureCanvasTkAgg(self.fig, master=plot_frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        # Right Column: Data & Inspector
        right_col = tk.Frame(main_frame, bg=C["bg"], width=500)
        right_col.pack(side="right", fill="both", padx=5)

        # UE Table
        self.ue_tree = self._create_tree(right_col, "Per-UE Performance", 
                                        ("ID", "IMSI", "Cell", "DL Mbps", "UL Mbps", "Loss %", "Dist", "RSRP"),
                                        (28, 38, 38, 60, 60, 50, 55, 55))

        # Raw Inspector
        debug_frame = tk.LabelFrame(right_col, text="Raw JSON Inspector", bg=C["card"])
        debug_frame.pack(fill="both", expand=True, pady=(10, 5))
        
        toolbar = tk.Frame(debug_frame, bg=C["card"])
        toolbar.pack(fill="x")
        tk.Checkbutton(toolbar, text="Pause Live View", variable=self.paused, bg=C["card"]).pack(side="left", padx=5)
        tk.Button(toolbar, text="Copy JSON", command=self._copy_to_clipboard, font=("Helvetica", 8)).pack(side="right", padx=5)

        self.raw_text = tk.Text(debug_frame, height=12, bg="#1e1e1e", fg="#d4d4d4", font=("Consolas", 9))
        self.raw_text.pack(fill="both", expand=True)

    def _create_stat_card(self, parent, title, value):
        frame = tk.Frame(parent, bg=C["card"], highlightthickness=1, highlightbackground=C["border"])
        frame.pack(side="left", expand=True, fill="both", padx=5)
        tk.Label(frame, text=title, bg=C["card"], fg=C["text_dim"], font=("Helvetica", 10)).pack(pady=(5,0))
        lbl_val = tk.Label(frame, text=value, bg=C["card"], fg=C["accent_blue"], font=("Helvetica", 14, "bold"))
        lbl_val.pack(pady=(0,5))
        return lbl_val

    def _create_tree(self, parent, title, cols, widths):
        frame = tk.LabelFrame(parent, text=title, bg=C["bg"])
        frame.pack(fill="both", expand=True)
        tree = ttk.Treeview(frame, columns=cols, show="headings", height=12)
        for col, width in zip(cols, widths):
            tree.heading(col, text=col)
            tree.column(col, width=width, anchor="center")
        tree.pack(fill="both", expand=True)
        return tree

    def _copy_to_clipboard(self):
        self.clipboard_clear()
        self.clipboard_append(self.raw_text.get(1.0, tk.END))
        messagebox.showinfo("Success", "Telemetry JSON copied.")

    def _refresh(self):
        state = None
        raw_msg = ""
        with self.receiver.lock:
            if self.receiver.latest_data:
                state = self.receiver.latest_data
                raw_msg = self.receiver.raw_message

        if raw_msg and not self.paused.get():
            self.raw_text.delete(1.0, tk.END)
            self.raw_text.insert(tk.END, json.dumps(json.loads(raw_msg), indent=2))

        if state:
            # Update Header & Progress
            sim_time = state['timestamp'].get('simulation_time', 0)
            self.lbl_time.config(text=f"Sim Time: {sim_time:.3f}s")
            self.progress['value'] = state['simulation'].get('progress_percent', 0)

            # Update Global Summary
            cfg = state.get('config', {})
            self.card_config.config(text=f"{cfg.get('gnb_count','?')} Cells | {cfg.get('ue_count','?')} UEs")
            
            ts = state.get('traffic_summary', {})
            dl_total = ts.get('total_dl_throughput_mbps') or 0
            ul_total = ts.get('total_ul_throughput_mbps') or 0
            loss_total = ts.get('avg_packet_loss_percent') or 0
            self.card_dl.config(text=f"{dl_total:.2f} Mbps")
            self.card_ul.config(text=f"{ul_total:.2f} Mbps")
            self.card_loss.config(text=f"{loss_total:.1f}%", fg=C["accent_red"] if loss_total > 20 else C["accent_green"])
            
            # Update Scheduler Type (from first gNB)
            gnbs = state.get('topology', {}).get('gnbs', [])
            if gnbs:
                sched_type = gnbs[0].get('scheduler', {}).get('type', '—')
                # Strip ns3:: prefix and shorten for display
                sched_display = sched_type.replace('ns3::NrMacScheduler', '').replace('ns3::', '')
                self.card_scheduler.config(text=sched_display if sched_display else '—')
            else:
                self.card_scheduler.config(text='—')

            # Clear Map and Table
            self.ax.cla()
            self.ax.grid(True, linestyle='--', alpha=0.3)
            for item in self.ue_tree.get_children(): self.ue_tree.delete(item)

            # Build gNB lookup for connection lines.
            # Key on every ID the telemetry might use (id, cell_id, plus str variants)
            # so the UE-side lookup hits regardless of type or naming.
            gnb_map = {}
            if 'topology' in state and 'gnbs' in state['topology']:
                for gnb in state['topology']['gnbs']:
                    gx, gy = gnb['position']['x'], gnb['position']['y']
                    pos = (gx, gy)
                    for key_field in ('id', 'cell_id', 'gnb_id'):
                        val = gnb.get(key_field)
                        if val is not None:
                            gnb_map[val] = pos
                            gnb_map[str(val)] = pos          # str variant
                    self.ax.plot(gx, gy, 'r^', markersize=12, markeredgecolor='black')
                    # Primary label: Cell ID
                    self.ax.text(gx, gy + 8, f"Cell {gnb.get('cell_id', gnb.get('id', '?'))}", 
                                color='red', weight='bold', ha='center', fontsize=9)
                    # Secondary label: Scheduler type (if available)
                    sched_type = gnb.get('scheduler', {}).get('type', '')
                    if sched_type and sched_type not in ('unknown', 'unavailable', 'no_device'):
                        sched_short = sched_type.replace('ns3::NrMacScheduler', '').replace('ns3::', '')
                        self.ax.text(gx, gy - 8, sched_short, 
                                    color='darkred', fontsize=7, ha='center', style='italic')

            # Draw UEs and Connection Lines
            for ue in state['topology'].get('ues', []):
                uid = ue['id']
                imsi = ue.get('imsi', '?')
                pos = ue.get('position', {'x':0, 'y':0})
                net = ue.get('network', {})
                traf = ue.get('traffic', {}).get('dl', {})
                
                ux, uy = pos['x'], pos['y']
                # Resolve serving gNB — try every field name the telemetry might emit.
                # Explicit None checks: do NOT use "or" because gnb_id=0 is valid but falsy.
                gnb_id = net.get('gnb_id')
                if gnb_id is None:
                    gnb_id = net.get('serving_gnb')
                if gnb_id is None:
                    gnb_id = net.get('cell_id')
                if gnb_id is None:
                    gnb_id = net.get('serving_cell_id')
                # Also try the string version in case of type mismatch
                if gnb_id not in gnb_map and gnb_id is not None:
                    gnb_id = str(gnb_id)
                
                # Draw Line to gNB — coloured to match UE health
                if gnb_id is not None and gnb_id in gnb_map:
                    gx, gy = gnb_map[gnb_id]
                    line_color = C["accent_green"] if (traf.get('throughput_mbps') or 0) >= 1.0 else \
                                 "#f59e0b"          if (traf.get('throughput_mbps') or 0) > 0.0  else \
                                 C["accent_red"]
                    self.ax.plot([ux, gx], [uy, gy], color=line_color, linestyle=':', linewidth=1.2, alpha=0.6, zorder=1)

                # Draw UE — colour by health: green = good, orange = weak, red = dead
                dl_mbps = traf.get('throughput_mbps') or 0
                loss_pct = traf.get('loss_percent') or 0
                
                # Extract UL traffic
                traf_ul = ue.get('traffic', {}).get('ul', {})
                ul_mbps = traf_ul.get('throughput_mbps') or 0
                
                if dl_mbps >= 1.0:
                    ue_color = C["accent_green"]
                elif dl_mbps > 0.0:
                    ue_color = "#f59e0b"   # amber
                else:
                    ue_color = C["accent_red"]
                self.ax.plot(ux, uy, 'o', color=ue_color, markersize=7, markeredgecolor='white', zorder=2)
                self.ax.text(ux + 3, uy + 3, f"UE {uid}", fontsize=8)

                # Extract radio metrics
                radio = ue.get('radio', {})
                rsrp_dbm = radio.get('rsrp_dbm', 0.0)
                has_radio = radio.get('available', False)
                rsrp_str = f"{rsrp_dbm:.1f}" if has_radio and rsrp_dbm != 0.0 else "—"

                # Update Table
                dist = net.get('distance_to_gnb') or 0
                self.ue_tree.insert("", "end", values=(uid, imsi, net.get('cell_id','-'), 
                                                       f"{dl_mbps:.2f}", f"{ul_mbps:.2f}", 
                                                       f"{loss_pct:.1f}", f"{dist:.1f}m", rsrp_str))

            # Pin axes to area bounds from config; fall back to 1000 if missing
            area = cfg.get('area_size') or state.get('topology', {}).get('area_size') or 1000
            self.ax.set_xlim(-20, area + 20)
            self.ax.set_ylim(-20, area + 20)
            self.ax.set_xlabel("X-Axis (meters)")
            self.ax.set_ylabel("Y-Axis (meters)")
            # Compact legend: green / amber / red
            from matplotlib.lines import Line2D
            legend_elements = [
                Line2D([0], [0], marker='o', color='w', markerfacecolor=C["accent_green"], markersize=9, label='Good (≥1 Mbps)'),
                Line2D([0], [0], marker='o', color='w', markerfacecolor='#f59e0b',        markersize=9, label='Weak  (<1 Mbps)'),
                Line2D([0], [0], marker='o', color='w', markerfacecolor=C["accent_red"],   markersize=9, label='Dead  (0 Mbps)'),
            ]
            self.ax.legend(handles=legend_elements, loc='lower right', fontsize=8, framealpha=0.9)
            self.canvas.draw()

        self.after(500, self._refresh) # 500ms refresh for stability

if __name__ == "__main__":
    app = TelemetryGUI(5555)
    app.mainloop()