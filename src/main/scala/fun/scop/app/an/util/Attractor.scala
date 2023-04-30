package fun.scop.app.an.util

case class Attractor(gs: Seq[GlobalState]) {
  override def toString = {
    gs.mkString("[", "->", "]")
  }
}
