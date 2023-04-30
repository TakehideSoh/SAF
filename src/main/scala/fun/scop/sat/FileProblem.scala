package fun.scop.sat

import java.io.IOException

case class FileProblem(cnfFile: java.io.File) {

  import java.io.{FileOutputStream, RandomAccessFile}
  import java.nio.ByteBuffer
  import java.nio.channels.FileChannel

  private[this] val SAT_BUFFER_SIZE = 256 * 1024
  private[this] val MAX_SAT_SIZE =
    10 * 1024 * 1024 * 1024L // 3 * 1024 * 1024 * 1024L
  var isDone = false

  var nofVariables = 0
  var nofClauses = 0
  var fileSize: Long = 0

  var nofVariablesCommitted = 0
  var nofClausesCommitted = 0
  var fileSizeCommitted: Long = 0

  private[this] var satFileChannel: Option[FileChannel] = None
  private[this] var satByteBuffer: Option[ByteBuffer] = None

  init

  /* set the number of variables*/
  def setNumberOfVariables(n: Int) =
    nofVariables = n

  /* */
  def setNumberOfClauses(n: Int) =
    nofClauses = n

  /* */
  def getAbsolutePath =
    cnfFile.getAbsolutePath

  /* */
  def open() = {
    if (satFileChannel.nonEmpty)
      throw new java.lang.Exception(
        "Internal error: re-opening file " + cnfFile.getAbsolutePath
      )
    try {
      if (fileSize == 0) {
        satFileChannel = Option(
          new FileOutputStream(cnfFile.getAbsolutePath).getChannel
        )
      } else {
        satFileChannel = Option(
          new RandomAccessFile(cnfFile.getAbsolutePath, "rw").getChannel
        )
        satFileChannel.get.position(fileSize)
      }
      satByteBuffer = Option(ByteBuffer.allocateDirect(SAT_BUFFER_SIZE))
    } catch {
      case e: IOException => throw new IOException
    }
  }

  /* */
  def write(b: Seq[Byte]): Unit = {
    if (satFileChannel.isEmpty)
      open()
    val len = b.size
    if (
      satByteBuffer
        .asInstanceOf[java.nio.ByteBuffer]
        .position() + len > SAT_BUFFER_SIZE
    )
      flush
    satByteBuffer.get.put(b.toArray)
    fileSize = fileSize + len
    if (fileSize >= MAX_SAT_SIZE)
      throw new Exception(
        "Encoding is interrupted because file size becomes too large (" + fileSize + " bytes)"
      )
  }

  /* */
  def write(s: String): Unit =
    write(s.getBytes)

  /* */
  def flush: Unit =
    satFileChannel match {
      case None => ()
      case Some(fc) =>
        try {
          // satByteBuffer.flip() // limit = position;  position = 0
          satByteBuffer.asInstanceOf[java.nio.Buffer].flip();
          // ((java.nio.Buffer) satByteBuffer).flip
          fc.write(satByteBuffer.get)
          // satByteBuffer.clear()
          satByteBuffer.asInstanceOf[java.nio.Buffer].clear();
        } catch {
          case e: IOException =>
            throw new IOException(
              "IOException is happen while flush FileProblem"
            )
        }
    }

  /* */
  def close: Unit =
    satFileChannel match {
      case None => ()
      case Some(fc) =>
        try {
          flush
          fc.close
          satFileChannel = None
          satByteBuffer = None
        } catch {
          case e: IOException =>
            throw new IOException(
              "IOException is happen while close FileProblem"
            )
        }
    }

  /* */
  def update: Unit = {
    val n = 64
    val s: StringBuilder = new StringBuilder
    s.append("p cnf ")
    s.append(nofVariables.toString)
    s.append(" ")
    s.append(nofClauses.toString)
    while (s.length < n - 1)
      s.append(" ");

    s.append("\n");
    val header = s.toString

    if (satFileChannel.nonEmpty) {
      throw new java.lang.Exception(
        "Internal error: updating opening file " + cnfFile.getAbsolutePath
      )
    }

    try {
      val satFile1: RandomAccessFile =
        new RandomAccessFile(cnfFile.getAbsolutePath, "rw")
      satFile1.seek(0)
      satFile1.write(header.getBytes())
      if (fileSize == 0)
        fileSize = header.length
      satFile1.setLength(fileSize)
      satFile1.close
    } catch {
      case e: IOException =>
        throw new IOException("IOException is happen while update FileProblem")
    }
  }

  /* */
  def init = {
    fileSize = 0
    nofVariables = 0
    nofClauses = 0
    isDone = false
    update
  }

  /* */
  def done() = {
    if (nofClauses == 0) {
      if (nofVariables == 0)
        nofVariables += 1
      addClause2CnfFile(Seq(1, -1))
      nofClauses += 1
    }
    isDone = true
    flush
    close
    update
  }

  /* */
  def addComment(comment: String) =
    write("c " + comment + "\n")

  /* */
  def commit = {
    nofVariablesCommitted = nofVariables
    nofClausesCommitted = nofClauses
    fileSizeCommitted = fileSize
  }

  /* */
  def rollback = {
    done
    nofVariables = nofVariablesCommitted
    nofClauses = nofClausesCommitted
    fileSize = fileSizeCommitted
    update
  }

  /* */
  def addClause2CnfFile(lits: Seq[Int]) =
    write(lits.mkString("", " ", " 0\n"))

}
