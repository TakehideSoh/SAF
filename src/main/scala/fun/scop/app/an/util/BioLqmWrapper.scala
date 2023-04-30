package fun.scop.app.an.util

import org.colomoto.biolqm.service.LQMServiceManager
import org.colomoto.biolqm.LogicalModel
import org.colomoto.biolqm.LQMLauncher

import org.colomoto.biolqm.io.bnet.BNetFormat
import org.colomoto.biolqm.io.booleannet.BooleanNetFormat
import org.colomoto.biolqm.io.sbml.SBMLFormat
import org.colomoto.biolqm.io.pint.PintFormat
import org.colomoto.biolqm.io.LogicalModelFormat

class BioLqmWrapper() {

  private def getFileExtension(fileName: String) =
    fileName.split('.').last

  private def getAnFileName(fileName: String) = {
    val init = fileName.split('.').init.mkString(".")
    init + ".an"
  }

  def convertOtherNetworkToAutomataNetwork(
      inputFilePath: String
  ) = {
    val inputFile = new java.io.File(inputFilePath)
    val inputFileName = inputFile.getName()
    val tmpFilePath = "/tmp/" + inputFileName
    val outputFilePath = getAnFileName(tmpFilePath)

    LQMLauncher.main(Array(inputFilePath, outputFilePath))

    outputFilePath
  }

  def convert(
      inputFilePath: String,
      inputFormat: String,
      outputFilePath: String
  ) = {
    val inFormat: LogicalModelFormat = inputFormat match {
      case "bnet"       => new BNetFormat()
      case "booleannet" => new BooleanNetFormat()
      case "sbml"       => new SBMLFormat()
    }
    val inModel = inFormat.load(inputFilePath)

    val outFormat = new PintFormat()

    outFormat.export(inModel, outputFilePath)
    outputFilePath
  }

  def convertOtherNetworkToAutomataNetwork(
      inputFilePath: String,
      inputFormat: String,
      outputFilePath: String,
      outputFormat: String
  ) = {
    // val inputFile = new java.io.File(inputFilePath)
    // val inputFileName = inputFile.getName()
    // val tmpFilePath = "/tmp/" + inputFileName
    // val outputFilePath = getAnFileName(tmpFilePath)

    LQMLauncher.main(
      Array(
        "-if",
        inputFormat,
        inputFilePath,
        "-of",
        outputFormat,
        outputFilePath
      )
    )

    outputFilePath
  }
}

object BioLqmWrapper {
  def main(args: Array[String]): Unit = {
    val converter = new BioLqmWrapper()

    converter.convertOtherNetworkToAutomataNetwork(args(0))

  }
}
