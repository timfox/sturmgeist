import sys
import subprocess
import os
import shutil
import stat
from PyQt6.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QPushButton, QTextEdit, QFileDialog, QLabel, QHBoxLayout, QMessageBox, QComboBox, QCheckBox
)
from PyQt6.QtGui import QFont, QFontDatabase
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

    def __init__(self, source_dir, build_dir, build_type, architecture, compiler_type, clean_build, enable_imgui,
                 build_client_mod=False, build_server_mod=False, build_mod_pk3=False, copy_mod_outputs=True):
        super().__init__()
        self.source_dir = source_dir
        self.build_dir = build_dir
        self.build_type = build_type
        self.architecture = architecture
        self.compiler_type = compiler_type
        self.clean_build = clean_build
        self.enable_imgui = enable_imgui
        self.build_client_mod = build_client_mod
        self.build_server_mod = build_server_mod
        self.build_mod_pk3 = build_mod_pk3
        self.copy_mod_outputs = copy_mod_outputs

    def run(self):
        try:
            # Clean build if requested
            if self.clean_build and os.path.exists(self.build_dir):
                self.output_signal.emit(f"Cleaning build directory: {self.build_dir}\n")
                # Ensure files/dirs are writable and remove read-only/locked files
                def _make_tree_writable(root_path: str) -> None:
                    for root, dirs, files in os.walk(root_path, topdown=False):
                        for name in files:
                            fp = os.path.join(root, name)
                            try:
                                os.chmod(fp, stat.S_IWRITE | stat.S_IREAD)
                            except Exception:
                                pass
                        for name in dirs:
                            dp = os.path.join(root, name)
                            try:
                                os.chmod(dp, stat.S_IWRITE | stat.S_IREAD | stat.S_IEXEC)
                            except Exception:
                                pass

                def _on_rm_error(func, path, exc_info):
                    # Clear read-only attribute then retry
                    try:
                        os.chmod(path, stat.S_IWRITE | stat.S_IREAD)
                        func(path)
                    except Exception:
                        raise

                try:
                    _make_tree_writable(self.build_dir)
                    shutil.rmtree(self.build_dir, onerror=_on_rm_error)
                except Exception as e:
                    # Fallback: try Windows command to force delete stubborn files (non-interactive)
                    try:
                        if sys.platform.startswith("win"):
                            subprocess.run(["cmd", "/c", f"attrib -r -s -h \"{self.build_dir}\\*\" /s /d"], check=False)
                            subprocess.run(["cmd", "/c", f"rmdir /s /q \"{self.build_dir}\""], check=True)
                        else:
                            raise
                    except Exception:
                        self.output_signal.emit(f"Failed to clean build directory: {e}\n")
                        self.finished_signal.emit(False)
                        return
            
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

            # Generator / compiler selection
            if sys.platform.startswith("win") and self.compiler_type.startswith("MSVC"):
                cmake_cmd.extend(["-G", "Visual Studio 17 2022"])
                vs_arch = "x64" if self.architecture == "x64" else "Win32"
                cmake_cmd.extend(["-A", vs_arch])
            else:
                # Default to Ninja for GCC/Clang
                cmake_cmd.extend(["-G", "Ninja"])
                if self.compiler_type.startswith("Clang"):
                    cmake_cmd.extend(["-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"])
            
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

            # Optional features
            if self.enable_imgui:
                cmake_cmd.append("-DFEATURE_IMGUI=ON")
                # Prefer bundled libs for simplicity
                cmake_cmd.append("-DBUNDLED_LIBS=ON")
                # Use system SDL2 to avoid build issues
                cmake_cmd.append("-DBUNDLED_SDL=OFF")

            # Gamecode build toggles
            if self.build_client_mod:
                cmake_cmd.append("-DBUILD_CLIENT_MOD=ON")
            if self.build_server_mod:
                cmake_cmd.append("-DBUILD_SERVER_MOD=ON")
            if self.build_mod_pk3:
                cmake_cmd.append("-DBUILD_MOD_PK3=ON")
            
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

            # If gamecode selected, build specific targets explicitly to ensure they are produced
            mod_targets = []
            if self.build_client_mod:
                mod_targets.extend(["ui", "cgame"])
            if self.build_server_mod:
                mod_targets.append("qagame")
            if self.build_mod_pk3:
                mod_targets.append("mod_pk3")

            if mod_targets:
                mod_cmd = [
                    "cmake", "--build", self.build_dir,
                    "--config", self.build_type,
                    "--target", *mod_targets
                ]
                self.output_signal.emit(f"\nRunning: {' '.join(mod_cmd)}\n")
                try:
                    proc = subprocess.Popen(
                        mod_cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        encoding=encoding,
                        errors="replace"
                    )
                except TypeError:
                    proc = subprocess.Popen(
                        mod_cmd,
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
                    self.output_signal.emit("Gamecode build failed.\n")
                    self.finished_signal.emit(False)
                    return

            # Optionally copy built mod outputs to dist/legacy for immediate run
            if self.copy_mod_outputs and (self.build_client_mod or self.build_server_mod or self.build_mod_pk3):
                try:
                    mod_build_dir = os.path.join(self.build_dir, "legacy")
                    mod_dist_dir = os.path.join(self.source_dir, "dist", "legacy")
                    if os.path.isdir(mod_build_dir):
                        os.makedirs(mod_dist_dir, exist_ok=True)
                        copied = 0
                        for name in os.listdir(mod_build_dir):
                            if not (name.endswith(".dll") or name.endswith(".pk3")):
                                continue
                            src = os.path.join(mod_build_dir, name)
                            dst = os.path.join(mod_dist_dir, name)
                            shutil.copy2(src, dst)
                            copied += 1
                        self.output_signal.emit(f"Copied {copied} gamecode files to {mod_dist_dir}\n")
                except Exception as e:
                    self.output_signal.emit(f"Warning: failed to copy mod outputs: {e}\n")

            self.output_signal.emit("\nBuild finished successfully.\n")
            self.finished_signal.emit(True)
        except Exception as e:
            self.output_signal.emit(f"Error: {e}\n")
            self.finished_signal.emit(False)

class CMakeBuilderGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Wolf Enemy Territory Engine Builder")
        self.setMinimumSize(700, 600)
        # Set Univers as the regular font for the whole application
        self.setFont(QFont("Univers"))
        self.init_ui()

    def init_ui(self):
        layout = QVBoxLayout()

        # Title banner
        title = QLabel("WOLFENSTEIN: Enemy Territory - Engine Builder")
        title.setAlignment(Qt.AlignmentFlag.AlignHCenter)
        title.setObjectName("titleLabel")
        # Try to load custom font for the word 'WOLFENSTEIN' only
        try:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            candidates = [
                os.path.join(script_dir, "wolfenstein.ttf"),
                os.path.join(script_dir, "assets", "wolfenstein.ttf"),
                os.path.join(script_dir, "assets", "fonts", "wolfenstein.ttf"),
                os.path.join(os.getcwd(), "wolfenstein.ttf"),
            ]
            font_family = None
            for path in candidates:
                if os.path.isfile(path):
                    fid = QFontDatabase.addApplicationFont(path)
                    if fid >= 0:
                        fams = QFontDatabase.applicationFontFamilies(fid)
                        if fams:
                            font_family = fams[0]
                            break
            if font_family:
                title.setTextFormat(Qt.TextFormat.RichText)
                title.setText(f"<span style=\"font-family: '{font_family}'; font-size: 64px; font-weight: regular;\">Wolfenstein</span>")
        except Exception:
            pass
        layout.addWidget(title)

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

        # Compiler selection
        comp_layout = QHBoxLayout()
        self.comp_label = QLabel("Compiler:")
        self.compiler_combo = QComboBox()
        self.compiler_combo.addItems(["GCC (MinGW)", "MSVC (Visual Studio)", "Clang"])
        comp_layout.addWidget(self.comp_label)
        comp_layout.addWidget(self.compiler_combo)
        layout.addLayout(comp_layout)

        # Clean build option
        self.clean_checkbox = QCheckBox("Clean build")
        self.clean_checkbox.setChecked(True)  # Default to clean build
        layout.addWidget(self.clean_checkbox)

        # Optional features
        self.imgui_checkbox = QCheckBox("Enable ImGui")
        self.imgui_checkbox.setChecked(True)
        layout.addWidget(self.imgui_checkbox)

        # Gamecode build options
        self.build_client_mod_checkbox = QCheckBox("Build client (ui, cgame)")
        self.build_client_mod_checkbox.setChecked(True)
        self.build_server_mod_checkbox = QCheckBox("Build server (qagame)")
        self.build_server_mod_checkbox.setChecked(False)
        self.build_mod_pk3_checkbox = QCheckBox("Package pk3")
        self.build_mod_pk3_checkbox.setChecked(False)
        self.copy_mod_outputs_checkbox = QCheckBox("Copy gamecode outputs to dist/legacy")
        self.copy_mod_outputs_checkbox.setChecked(True)
        layout.addWidget(self.build_client_mod_checkbox)
        layout.addWidget(self.build_server_mod_checkbox)
        layout.addWidget(self.build_mod_pk3_checkbox)
        layout.addWidget(self.copy_mod_outputs_checkbox)

        # Action buttons
        actions_layout = QHBoxLayout()
        self.build_button = QPushButton("Build")
        self.build_button.clicked.connect(self.start_build)
        self.save_log_button = QPushButton("Save Log")
        self.save_log_button.clicked.connect(self.save_log)
        actions_layout.addWidget(self.build_button)
        actions_layout.addStretch(1)
        actions_layout.addWidget(self.save_log_button)
        layout.addLayout(actions_layout)

        # Output log
        self.output_log = QTextEdit()
        self.output_log.setReadOnly(True)
        self.output_log.setFont(QFont("Consolas", 10))
        layout.addWidget(self.output_log)

        self.setLayout(layout)
        self.build_thread = None

        self.apply_theme()

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
        compiler_type = self.compiler_combo.currentText()
        clean_build = self.clean_checkbox.isChecked()
        enable_imgui = self.imgui_checkbox.isChecked()
        build_client_mod = self.build_client_mod_checkbox.isChecked()
        build_server_mod = self.build_server_mod_checkbox.isChecked()
        build_mod_pk3 = self.build_mod_pk3_checkbox.isChecked()
        copy_mod_outputs = self.copy_mod_outputs_checkbox.isChecked()

        if not os.path.isfile(os.path.join(source_dir, "CMakeLists.txt")):
            QMessageBox.critical(self, "Error", "CMakeLists.txt not found in source directory.")
            return

        self.output_log.clear()
        self.build_button.setEnabled(False)
        self.build_thread = CMakeBuildThread(source_dir, build_dir, build_type, architecture, compiler_type, clean_build,
                                            enable_imgui, build_client_mod, build_server_mod, build_mod_pk3, copy_mod_outputs)
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

    def save_log(self):
        path, _ = QFileDialog.getSaveFileName(self, "Save Build Log", os.path.join(os.getcwd(), "build.log"), "Text Files (*.txt);;All Files (*)")
        if path:
            try:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(self.output_log.toPlainText())
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to save log: {e}")

    def apply_theme(self):
        # Simple Wolfenstein-inspired dark theme
        self.setStyleSheet(
            """
            QWidget { background-color: #1a0f0f; color: #e8e0d5; font-family: 'Univers', sans-serif; }
            #titleLabel { color: #e8e0d5; font-size: 18px; font-weight: bold; padding: 8px; border: 2px solid #7a0000; border-radius: 6px; background-color: #2a1414; }
            QLabel { color: #e8e0d5; }
            QLineEdit, QTextEdit { background-color: #121212; color: #e8e0d5; border: 1px solid #7a0000; border-radius: 4px; }
            QComboBox { background-color: #121212; color: #e8e0d5; border: 1px solid #7a0000; border-radius: 4px; padding: 2px; }
            QPushButton { background-color: #7a0000; color: #ffffff; border: 1px solid #b30000; border-radius: 4px; padding: 6px 10px; }
            QPushButton:hover { background-color: #8c0000; }
            QPushButton:disabled { background-color: #3a3a3a; border-color: #555; }
            QCheckBox { color: #e8e0d5; background-color: #121212; border: 1px solid #7a0000; border-radius: 4px; padding: 4px 8px; }
            QCheckBox::indicator { width: 18px; height: 18px; border-radius: 3px; background: #1a0f0f; border: 1px solid #7a0000; }
            QCheckBox::indicator:checked { background: #7a0000; border: 1px solid #b30000; }
            QCheckBox::indicator:unchecked { background: #121212; border: 1px solid #7a0000; }
            """
        )

def main():
    app = QApplication(sys.argv)
    # Set Univers as the regular font for the whole application
    app.setFont(QFont("Univers"))
    window = CMakeBuilderGUI()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
