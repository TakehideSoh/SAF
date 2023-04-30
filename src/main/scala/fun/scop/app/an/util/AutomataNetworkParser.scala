package fun.scop.app.an.util

import scala.util.parsing.combinator._

case class AutomataNetworkParser(inputFile: String) extends JavaTokenParsers {

  var as = IndexedSeq.empty[Automata]
  var map = Map.empty[Automata, Seq[Int]]
  var ts = IndexedSeq.empty[Transition]

  def parse() = {

    val src = scala.io.Source.fromFile(inputFile)
    val empty = """\s*""".r

    var flag = false

    for (
      line <- src.getLines()
      if !line.startsWith("initial_context") && !line.startsWith("#")
    ) {
      if (line.matches("""\(\*\*.*""")) flag = true
      if (line.matches("""\(\*.*""")) flag = true

      if (!flag) {
        line match {
          case empty() =>
          case _ => {
            val str = line.trim.split(';').head
            if (!parseAll(aNetworkDefinition, str).toString.contains("parsed"))
              throw new Exception(s"Parsing error: $line")
          }
        }
      }

      if (line.matches("""\s*\*\*\).*""")) flag = false
      if (line.matches("""\s*\*\).*""")) flag = false

    }

    AutomataNetwork(as, map, ts)
  }

  def symbols = """[-_a-zA-Z0-9]+""".r

  def name: Parser[String] = (""""""" ~> symbols) <~ """"""" ^^ { case ss =>
    ss
  }

  def any = """.*""".r

  def lstate: Parser[Seq[Int]] =
    "[" ~> wholeNumber ~ rep("," ~> wholeNumber) <~ "]" ^^ { case head ~ tail =>
      // head.toInt +: tail.map(_.toInt)
      (head.toInt to tail.last.toInt).toSeq
    }

  // "ERBB1_3" [0,1]
  // "CellCycleArrest" [0, 1]
  def automataDefinition: Parser[Any] = name ~ lstate ^^ {
    case n ~ ls => {
      val a = Automata(n)
      as = as :+ a
      map += a -> ls
    }
  }

  def conditionr = """"(.*)"=(\d+)""".r

  def condition: Parser[AutomataValued] = (name <~ "=") ~ wholeNumber ^^ {
    case n ~ v => AutomataValued(Automata(n), v.toInt)
  }

  // "CycD1" 0 -> 1 when "AKT1"=1 and "ERalpha"=1 and "MYC"=1

  def transitionDefinitionWithCondition: Parser[Any] =
    name ~ (wholeNumber <~ "->") ~ wholeNumber ~ rep("when" ~> condition) ~ rep(
      "and" ~> condition
    ) ^^ {
      case n ~ v0 ~ v1 ~ vd ~ vs => {
        val a = Automata(n)
        val origin = AutomataValued(a, v0.toInt)
        val destination = AutomataValued(a, v1.toInt)
        val conditions = if (vd.isEmpty) Seq.empty else vd ++ vs

        ts = ts :+ Transition(origin, destination, conditions)
      }
    }

  def transitionDefinitionWithoutCondition: Parser[Any] =
    name ~ (wholeNumber <~ "->") ~ wholeNumber ^^ {
      case n ~ v0 ~ v1 => {
        val a = Automata(n)
        val origin = AutomataValued(a, v0.toInt)
        val destination = AutomataValued(a, v1.toInt)

        ts = ts :+ Transition(origin, destination, Seq.empty)
      }
    }

  //  def transitionDefinition: Parser[Any] = name ~ wholeNumber ~ any

  def aNetworkDefinition: Parser[Any] =
    automataDefinition | transitionDefinitionWithCondition | transitionDefinitionWithoutCondition

}
