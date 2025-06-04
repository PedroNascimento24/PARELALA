import tkinter as tk
from tkinterdnd2 import DND_FILES, TkinterDnD
from tkinter import filedialog
from solver import process_file
from c_output_gantt import c_output_to_schedule
from chart import create_gantt_chart

def main():
    """
    Main function to create a Tkinter GUI for a Jobshop Problem Solver application.
    This function sets up a Tkinter window with drag-and-drop support and a file selection dialog.
    It creates a semi-transparent black window with centered content, including instructions,
    an up-arrow label, and a text widget to display the path of the dragged or selected file.
    The function defines two callback functions:
    - drop(event): Handles the event when a file is dropped onto the window.
    - select_file(event=None): Opens a file dialog to select a file from the computer.
    The function binds the drop event to the root window and a mouse click event to open the file dialog.
    Finally, it runs the Tkinter event loop to keep the GUI active.
    """
    """Main function to create Tkinter GUI."""
    # Create a TkinterDnD.Tk() window with DnD (Drag and Drop) support
    root = TkinterDnD.Tk()
    root.title("Jobshop Problem Solver")
    root.geometry("600x400")
    root.configure(bg="black")
    root.attributes('-alpha', 0.85)  # Set window to semi-transparent black

    # Create a main frame for vertical centering
    main_frame = tk.Frame(root, bg="black")
    main_frame.pack(expand=True, fill="both")

    # Create a secondary frame inside the main frame for content centering
    content_frame = tk.Frame(main_frame, bg="black")
    content_frame.pack(expand=True, fill="none")

    # Define label for instructions with centered text
    label = tk.Label(content_frame, text="Drag File \nor\n Select from Computer", font=("Arial", 18), fg="white", bg="black")
    label.pack(pady=5)

    # Define label for the up-arrow (wider)
    arrow_label = tk.Label(content_frame, text="⬆", font=("Arial", 70), fg="white", bg="black")
    arrow_label.pack(pady=10)

    # Define text widget to show dragged file
    text_widget = tk.Text(content_frame, height=5, width=50, wrap='word', font=("Arial", 12), bg="black", fg="white", bd=0)
    text_widget.pack(pady=10)

    def drop(event):
        """Callback function for when a file is dropped."""
        file_path = event.data.strip("{}")
        text_widget.delete('1.0', tk.END)
        try:
            with open(file_path, 'r') as f:
                first_line = f.readline().strip()
                tokens = first_line.split()
            if len(tokens) == 1 and tokens[0].isdigit():
                text_widget.insert(tk.END, f"Visualizing output: {file_path}\n")
                print("[DEBUG] Detected C output format. Calling c_output_to_schedule...")
                schedule = c_output_to_schedule(file_path)
                output_image = file_path + '_gantt.png'
                print(f"[DEBUG] Creating Gantt chart at: {output_image}")
                create_gantt_chart(schedule, output_image)
                text_widget.insert(tk.END, f"Gantt chart saved to {output_image}\n")
                import os
                try:
                    os.startfile(output_image)
                except Exception as e:
                    print(f"[DEBUG] Could not open image automatically: {e}")
                    text_widget.insert(tk.END, f"\n[Info] Could not open image automatically: {e}")
                return
            else:
                text_widget.insert(tk.END, f"Processing dataset file: {file_path}\n")
                process_file(file_path)
        except Exception as e:
            print(f"[DEBUG] Exception: {e}")
            text_widget.insert(tk.END, f"Error processing file: {e}\n\nPlease ensure the file is in the correct format.")

    def select_file(event=None):
        """Function to select a file from the computer or handle drag-and-drop."""
        file_path = filedialog.askopenfilename(
            title="Select Output File",
            filetypes=[("Text Files", "*.txt"), ("All Files", "*.*")]
        )
        if file_path:
            text_widget.delete('1.0', tk.END)
            try:
                # Try to parse as C output file (first line: makespan, rest: start,duration)
                with open(file_path, 'r') as f:
                    first_line = f.readline().strip()
                    tokens = first_line.split()
                if len(tokens) == 1 and tokens[0].isdigit():
                    text_widget.insert(tk.END, f"Visualizing output: {file_path}\n")
                    print("[DEBUG] Detected C output format. Calling c_output_to_schedule...")
                    schedule = c_output_to_schedule(file_path)
                    output_image = file_path + '_gantt.png'
                    print(f"[DEBUG] Creating Gantt chart at: {output_image}")
                    create_gantt_chart(schedule, output_image)
                    text_widget.insert(tk.END, f"Gantt chart saved to {output_image}\n")
                    import os
                    try:
                        os.startfile(output_image)
                    except Exception as e:
                        print(f"[DEBUG] Could not open image automatically: {e}")
                        text_widget.insert(tk.END, f"\n[Info] Could not open image automatically: {e}")
                    return
                else:
                    # If the first line has two or more integers, treat as dataset for Python solver
                    text_widget.insert(tk.END, f"Processing dataset file: {file_path}\n")
                    process_file(file_path)
            except Exception as e:
                print(f"[DEBUG] Exception: {e}")
                text_widget.insert(tk.END, f"Error processing file: {e}\n\nPlease ensure the file is in the correct format.")

    # Bind the drop event to the root window
    root.drop_target_register(DND_FILES)
    root.dnd_bind('<<Drop>>', drop)

    # Bind mouse click to file selection (for both dataset and C output files)
    root.bind("<Button-1>", select_file)

    # Run the Tkinter event loop
    root.mainloop()

if __name__ == "__main__":
    main()