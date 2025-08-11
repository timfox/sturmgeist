import sys
import subprocess
import os
import shutil
from PyQt6.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QPushButton, QTextEdit, QFileDialog, QLabel, QHBoxLayout, QMessageBox, QComboBox, QCheckBox
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal

def get_preferred_encoding():
    # Try to get the preferred encoding for the system, fallback to utf-8
    import locale
    encoding = locale.getpreferredencoding(False)
    if not encoding:
        encoding = "utf-8"
    return encoding

class CMakeBuildThread(QThread):
    output_signal = pyqtSignal(str)
    finished_signal = pyqtSignal(bool)

    def __init__(self, source_dir, build_dir, build_type, architecture, clean_build):
        super().__init__()
        self.source_dir = source_dir
        self.build_dir = build_dir
        self.build_type = build_type
        self.architecture = architecture
        self.clean_build = clean_build

    def run(self):
        try:
            # Clean build if requested
            if self.clean_build and os.path.exists(self.build_dir):
                self.output_signal.emit(f"Cleaning build directory: {self.build_dir}\n")
                shutil.rmtree(self.build_dir)
            
            # Ensure build directory exists
            if not os.path.exists(self.build_dir):
                os.makedirs(self.build_dir)
            
            # Run cmake configure
            cmake_cmd = [
                "cmake",
                "-S", self.source_dir,
                "-B", self.build_dir,
                f"-DCMAKE_BUILD_TYPE={self.build_type}"
            ]
            
            # Add architecture flag
            if self.architecture == "x86":
                cmake_cmd.append("-DCROSS_COMPILE32=ON")
            else:  # x64
                cmake_cmd.append("-DCROSS_COMPILE32=OFF")
            
            # Disable downloads for offline compilation
            cmake_cmd.extend([
                "-DINSTALL_GEOIP=OFF",
                "-DINSTALL_WOLFADMIN=OFF", 
                "-DINSTALL_EXTRA=OFF"
            ])
            
            self.output_signal.emit(f"Running: {' '.join(cmake_cmd)}\n")
            encoding = get_preferred_encoding()
            try:
                proc = subprocess.Popen(
                    cmake_cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    encoding=encoding,
                    errors="replace"
                )
            except TypeError:
                # For Python <3.6 compatibility
                proc = subprocess.Popen(
                    cmake_cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT
                )
            for raw_line in proc.stdout:
                # If text mode, already decoded; else, decode
                if isinstance(raw_line, bytes):
                    line = raw_line.decode(encoding, errors="replace")
                else:
                    line = raw_line
                self.output_signal.emit(line)
            proc.wait()
            if proc.returncode != 0:
                self.output_signal.emit("CMake configuration failed.\n")
                self.finished_signal.emit(False)
                return

            # Run cmake build
            build_cmd = [
                "cmake",
                "--build", self.build_dir,
                "--config", self.build_type
            ]
            self.output_signal.emit(f"\nRunning: {' '.join(build_cmd)}\n")
            try:
                proc = subprocess.Popen(
                    build_cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    encoding=encoding,
                    errors="replace"
                )
            except TypeError:
                proc = subprocess.Popen(
                    build_cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT
                )
            for raw_line in proc.stdout:
                if isinstance(raw_line, bytes):
                    line = raw_line.decode(encoding, errors="replace")
                else:
                    line = raw_line
                self.output_signal.emit(line)
            proc.wait()
            if proc.returncode != 0:
                self.output_signal.emit("Build failed.\n")
                self.finished_signal.emit(False)
                return

            self.output_signal.emit("\nBuild finished successfully.\n")
            self.finished_signal.emit(True)
        except Exception as e:
            self.output_signal.emit(f"Error: {e}\n")
            self.finished_signal.emit(False)

class CMakeBuilderGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ET: Legacy CMake Builder")
        self.setMinimumSize(700, 600)
        self.init_ui()

    def init_ui(self):
        layout = QVBoxLayout()

        # Source dir selection
        src_layout = QHBoxLayout()
        self.src_label = QLabel("Source Directory:")
        self.src_path = QTextEdit(os.getcwd())
        self.src_path.setMaximumHeight(30)
        self.src_browse = QPushButton("Browse")
        self.src_browse.clicked.connect(self.browse_source)
        src_layout.addWidget(self.src_label)
        src_layout.addWidget(self.src_path)
        src_layout.addWidget(self.src_browse)
        layout.addLayout(src_layout)

        # Build dir selection
        build_layout = QHBoxLayout()
        self.build_label = QLabel("Build Directory:")
        self.build_path = QTextEdit(os.path.join(os.getcwd(), "build"))
        self.build_path.setMaximumHeight(30)
        self.build_browse = QPushButton("Browse")
        self.build_browse.clicked.connect(self.browse_build)
        build_layout.addWidget(self.build_label)
        build_layout.addWidget(self.build_path)
        build_layout.addWidget(self.build_browse)
        layout.addLayout(build_layout)

        # Build type selection
        type_layout = QHBoxLayout()
        self.type_label = QLabel("Build Type:")
        self.type_combo = QComboBox()
        self.type_combo.addItems(["Release", "Debug", "RelWithDebInfo", "MinSizeRel"])
        type_layout.addWidget(self.type_label)
        type_layout.addWidget(self.type_combo)
        layout.addLayout(type_layout)

        # Architecture selection
        arch_layout = QHBoxLayout()
        self.arch_label = QLabel("Architecture:")
        self.arch_combo = QComboBox()
        self.arch_combo.addItems(["x64", "x86"])
        self.arch_combo.setCurrentText("x64")  # Default to 64-bit
        arch_layout.addWidget(self.arch_label)
        arch_layout.addWidget(self.arch_combo)
        layout.addLayout(arch_layout)

        # Clean build option
        self.clean_checkbox = QCheckBox("Clean build (removes build directory first)")
        self.clean_checkbox.setChecked(True)  # Default to clean build
        layout.addWidget(self.clean_checkbox)

        # Build button
        self.build_button = QPushButton("Build")
        self.build_button.clicked.connect(self.start_build)
        layout.addWidget(self.build_button)

        # Output log
        self.output_log = QTextEdit()
        self.output_log.setReadOnly(True)
        layout.addWidget(self.output_log)

        self.setLayout(layout)
        self.build_thread = None

    def browse_source(self):
        dir = QFileDialog.getExistingDirectory(self, "Select Source Directory", self.src_path.toPlainText())
        if dir:
            self.src_path.setText(dir)

    def browse_build(self):
        dir = QFileDialog.getExistingDirectory(self, "Select Build Directory", self.build_path.toPlainText())
        if dir:
            self.build_path.setText(dir)

    def start_build(self):
        source_dir = self.src_path.toPlainText().strip()
        build_dir = self.build_path.toPlainText().strip()
        build_type = self.type_combo.currentText()
        architecture = self.arch_combo.currentText()
        clean_build = self.clean_checkbox.isChecked()

        if not os.path.isfile(os.path.join(source_dir, "CMakeLists.txt")):
            QMessageBox.critical(self, "Error", "CMakeLists.txt not found in source directory.")
            return

        self.output_log.clear()
        self.build_button.setEnabled(False)
        self.build_thread = CMakeBuildThread(source_dir, build_dir, build_type, architecture, clean_build)
        self.build_thread.output_signal.connect(self.append_output)
        self.build_thread.finished_signal.connect(self.build_finished)
        self.build_thread.start()

    def append_output(self, text):
        self.output_log.append(text)
        self.output_log.verticalScrollBar().setValue(self.output_log.verticalScrollBar().maximum())

    def build_finished(self, success):
        self.build_button.setEnabled(True)
        if success:
            QMessageBox.information(self, "Build", "Build finished successfully.")
        else:
            QMessageBox.critical(self, "Build", "Build failed. See output for details.")

def main():
    app = QApplication(sys.argv)
    window = CMakeBuilderGUI()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
