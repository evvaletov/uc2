class Uc2 < Formula
  desc "Read and create UltraCompressor II archives"
  homepage "https://github.com/evvaletov/uc2"
  url "https://github.com/evvaletov/uc2/archive/refs/tags/v3.0.0-alpha.13.tar.gz"
  version "3.0.0a13"
  sha256 "41566267ca92d36cf181b6c4fc5878b53050b4aebf43baba7d16bdf60149ed93"
  license all_of: ["GPL-3.0-or-later", "LGPL-3.0-only", "BSD-2-Clause"]

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args,
           "-DUC2_VERSION_LABEL=#{version}", "-DUC2_BUILD_TESTS=OFF"
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
    pkgshare.install "LICENSE", "LICENSES", "CREDITS.md"
  end

  test do
    payload = "UC2 package round trip\n" * 100
    (testpath/"input.txt").write payload
    system bin/"uc2", "-qw", "-L4", "classic.uc2", "input.txt"
    system bin/"uc2", "-t", "classic.uc2"
    assert_equal payload, shell_output("#{bin}/uc2 -p classic.uc2")
    system bin/"uc2", "-qw", "-L9", "--chunked", "modern.uc2", "input.txt"
    system bin/"uc2", "-t", "modern.uc2"
    assert_equal payload, shell_output("#{bin}/uc2 -p modern.uc2")
    assert_match "UC2 #{version}", shell_output("#{bin}/uc2 -h")
  end
end
